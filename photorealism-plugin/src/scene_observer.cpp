#include "scene_observer.hpp"

#include "runtime.hpp"
#include "scene_formats.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace photorealism {
namespace {
constexpr unsigned kTargetSampleWidth = 96u;

bool supported_format(DXGI_FORMAT format) {
    return scene_formats::is_readable(static_cast<unsigned>(format));
}

bool format_is_bgra(DXGI_FORMAT format) {
    return scene_formats::is_bgra(static_cast<unsigned>(format));
}

DXGI_FORMAT sample_format(DXGI_FORMAT format) {
    return static_cast<DXGI_FORMAT>(
        scene_formats::resolve_unorm(static_cast<unsigned>(format)));
}

unsigned long long now_ms() {
    return static_cast<unsigned long long>(GetTickCount64());
}
}

void SceneObserver::configure(
    bool enabled, unsigned interval_frames, float log_seconds) {
    enabled_ = enabled;
    interval_frames_ = interval_frames < 1u ? 1u : interval_frames;
    log_seconds_ = log_seconds < 0.0f ? 0.0f : log_seconds;

    first_measurement_logged_ = false;
}

void SceneObserver::release() {
    for (Slot& slot : slots_) {
        if (slot.staging != nullptr) {
            slot.staging->Release();
            slot.staging = nullptr;
        }
        if (slot.completion != nullptr) {
            slot.completion->Release();
            slot.completion = nullptr;
        }
        slot.pending = false;
    }
    if (pyramid_view_ != nullptr) {
        pyramid_view_->Release();
        pyramid_view_ = nullptr;
    }
    if (pyramid_ != nullptr) {
        pyramid_->Release();
        pyramid_ = nullptr;
    }
    source_width_ = 0u;
    source_height_ = 0u;
    source_format_ = DXGI_FORMAT_UNKNOWN;
    sample_format_ = DXGI_FORMAT_UNKNOWN;
    resources_failed_ = false;
    latest_ = SceneFeatures{};
}

bool SceneObserver::ensure_resources(
    ID3D11Device* device, ID3D11Texture2D* scene) {
    D3D11_TEXTURE2D_DESC description = {};
    scene->GetDesc(&description);

    if (description.Width == source_width_ &&
        description.Height == source_height_ &&
        description.Format == source_format_) {
        return !resources_failed_;
    }
    release();
    if (!supported_format(description.Format) ||
        description.SampleDesc.Count != 1u || description.Width == 0u ||
        description.Height == 0u) {
        log_message(
            "Observador de cena 0.18.0 inativo: formato %u ou MSAA %u nao "
            "suportados para leitura.",
            static_cast<unsigned>(description.Format),
            static_cast<unsigned>(description.SampleDesc.Count));
        source_width_ = description.Width;
        source_height_ = description.Height;
        source_format_ = description.Format;
        resources_failed_ = true;
        return false;
    }
    const DXGI_FORMAT readable = sample_format(description.Format);

    D3D11_TEXTURE2D_DESC pyramid = description;
    pyramid.MipLevels = 0u;
    pyramid.Usage = D3D11_USAGE_DEFAULT;
    pyramid.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    pyramid.CPUAccessFlags = 0u;
    pyramid.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    pyramid.Format = readable;
    if (FAILED(device->CreateTexture2D(&pyramid, nullptr, &pyramid_))) {
        log_message(
            "Observador de cena 0.18.0 inativo: piramide %ux%u nao pode ser "
            "criada.",
            description.Width,
            description.Height);
        resources_failed_ = true;
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC pyramid_view = {};
    pyramid_view.Format = readable;
    pyramid_view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    pyramid_view.Texture2D.MostDetailedMip = 0u;
    pyramid_view.Texture2D.MipLevels = static_cast<UINT>(-1);
    if (FAILED(device->CreateShaderResourceView(
            pyramid_, &pyramid_view, &pyramid_view_))) {
        log_message(
            "Observador de cena 0.18.0 inativo: view da piramide recusada.");
        release();
        resources_failed_ = true;
        return false;
    }

    D3D11_TEXTURE2D_DESC created = {};
    pyramid_->GetDesc(&created);
    mip_level_ = 0u;
    mip_width_ = description.Width;
    mip_height_ = description.Height;
    for (unsigned level = 0u; level < created.MipLevels; ++level) {
        const unsigned width = std::max(description.Width >> level, 1u);
        const unsigned height = std::max(description.Height >> level, 1u);
        mip_level_ = level;
        mip_width_ = width;
        mip_height_ = height;
        if (width <= kTargetSampleWidth) {
            break;
        }
    }

    D3D11_TEXTURE2D_DESC staging = {};
    staging.Width = mip_width_;
    staging.Height = mip_height_;
    staging.MipLevels = 1u;
    staging.ArraySize = 1u;
    staging.Format = readable;
    staging.SampleDesc.Count = 1u;
    staging.Usage = D3D11_USAGE_STAGING;
    staging.BindFlags = 0u;
    staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging.MiscFlags = 0u;
    D3D11_QUERY_DESC query = {D3D11_QUERY_EVENT, 0u};
    for (Slot& slot : slots_) {
        if (FAILED(device->CreateTexture2D(&staging, nullptr, &slot.staging)) ||
            FAILED(device->CreateQuery(&query, &slot.completion))) {
            log_message(
                "Observador de cena 0.18.0 inativo: staging %ux%u recusado.",
                mip_width_,
                mip_height_);
            release();
            resources_failed_ = true;
            return false;
        }
        slot.pending = false;
    }

    source_width_ = description.Width;
    source_height_ = description.Height;
    source_format_ = description.Format;
    sample_format_ = readable;
    resources_failed_ = false;
    log_message(
        "Observador de cena 0.18.0 ativo: fonte %ux%u format=%u amostrado como "
        "%u, mip %u (%ux%u = %u pixels), intervalo=%u frames, log=%.0fs.",
        source_width_,
        source_height_,
        static_cast<unsigned>(source_format_),
        static_cast<unsigned>(sample_format_),
        mip_level_,
        mip_width_,
        mip_height_,
        mip_width_ * mip_height_,
        interval_frames_,
        static_cast<double>(log_seconds_));
    return true;
}

void SceneObserver::drain(ID3D11DeviceContext* context) {
    for (Slot& slot : slots_) {
        if (!slot.pending) {
            continue;
        }

        if (context->GetData(
                slot.completion,
                nullptr,
                0u,
                D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
            continue;
        }
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(context->Map(
                slot.staging, 0u, D3D11_MAP_READ, 0u, &mapped)) &&
            mapped.pData != nullptr) {
            compute(
                static_cast<const unsigned char*>(mapped.pData),
                mapped.RowPitch);
            context->Unmap(slot.staging, 0u);
        }
        slot.pending = false;
    }
}

void SceneObserver::compute(const unsigned char* pixels, unsigned pitch) {
    const SceneFeatures features = compute_scene_features(
        pixels,
        mip_width_,
        mip_height_,
        pitch,
        format_is_bgra(sample_format_));
    if (!features.valid) {
        return;
    }
    latest_ = features;

    const unsigned long long now = now_ms();
    const bool due =
        log_seconds_ > 0.0f &&
        now - last_log_ms_ >=
            static_cast<unsigned long long>(log_seconds_ * 1000.0f);
    if (!first_measurement_logged_ || due) {
        log_message(
            "Cena 0.18.0: ceu_R/B=%.3f mediana=%.1f faixa_p90-p10=%.1f "
            "saturacao=%.3f media=%.1f amostra=%u.",
            static_cast<double>(features.sky_r_over_b),
            static_cast<double>(features.median),
            static_cast<double>(features.dynamic_range),
            static_cast<double>(features.saturation),
            static_cast<double>(features.mean),
            features.sample_pixels);
        last_log_ms_ = now;
        first_measurement_logged_ = true;
    }
}

void SceneObserver::observe(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* scene) {
    if (!enabled_ || device == nullptr || context == nullptr ||
        scene == nullptr) {
        return;
    }
    if (!ensure_resources(device, scene)) {
        return;
    }

    drain(context);

    ++frame_counter_;
    if (frame_counter_ < interval_frames_) {
        return;
    }
    frame_counter_ = 0u;

    Slot& slot = slots_[next_slot_];
    next_slot_ = (next_slot_ + 1u) % 2u;
    if (slot.pending) {
        return;
    }

    context->CopySubresourceRegion(
        pyramid_, 0u, 0u, 0u, 0u, scene, 0u, nullptr);
    context->GenerateMips(pyramid_view_);
    context->CopySubresourceRegion(
        slot.staging, 0u, 0u, 0u, 0u, pyramid_, mip_level_, nullptr);
    context->End(slot.completion);
    slot.pending = true;
}
}
