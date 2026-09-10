#include "sampler.hpp"

#include "../runtime.hpp"
#include "formats.hpp"

#include <algorithm>

namespace photorealism {
namespace {
constexpr unsigned kTargetSampleWidth = 96u;

bool supported_format(DXGI_FORMAT format) {
    return scene_formats::is_readable(static_cast<unsigned>(format));
}


DXGI_FORMAT sample_format(DXGI_FORMAT format) {
    return static_cast<DXGI_FORMAT>(
        scene_formats::resolve_unorm(static_cast<unsigned>(format)));
}

}

void SceneSampler::release() {
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
    creation_notice_ = false;
    resources_failed_ = false;
}

bool SceneSampler::ensure_resources(
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
    creation_notice_ = true;
    return true;
}

}
