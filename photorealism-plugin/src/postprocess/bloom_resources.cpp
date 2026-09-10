#include "bloom_pyramid.hpp"

#include "../runtime.hpp"
#include "com_utils.hpp"
#include "format_utils.hpp"

#include <algorithm>

namespace photorealism {

bool BloomPyramid::attach(ID3D11Device* device, ID3D11DeviceContext* context) {
    device_ = device;
    context_ = context;
    D3D11_BUFFER_DESC description = {};
    description.ByteWidth = sizeof(BloomConstants);
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    return SUCCEEDED(
        device_->CreateBuffer(&description, nullptr, &constant_buffer_));
}

void BloomPyramid::release_constant_buffer() {
    safe_release(constant_buffer_);
}

UINT BloomPyramid::levels_for_radius(float radius, UINT height) {
    if (radius <= 0.0f || height == 0) {
        return 1;
    }
    const float target = radius * static_cast<float>(height) / 1.5f;
    UINT levels = 1;
    while (levels < kMaxLevels &&
           static_cast<float>(1u << levels) < target) {
        ++levels;
    }
    return levels;
}

void BloomPyramid::release() {
    for (UINT index = 0; index < kMaxLevels; ++index) {
        safe_release(targets_[index]);
        safe_release(views_[index]);
        safe_release(textures_[index]);
        widths_[index] = 0;
        heights_[index] = 0;
    }
    level_count_ = 0;
}

bool BloomPyramid::ensure(
    const D3D11_TEXTURE2D_DESC& source, UINT requested_levels) {
    if (level_count_ == requested_levels &&
        textures_[0] != nullptr &&
        widths_[0] == std::max(source.Width >> 1, 1u) &&
        heights_[0] == std::max(source.Height >> 1, 1u)) {
        return true;
    }

    release();

    HRESULT result = S_OK;
    for (UINT index = 0; index < requested_levels; ++index) {
        const UINT level_width =
            std::max(source.Width >> (index + 1), 1u);
        const UINT level_height =
            std::max(source.Height >> (index + 1), 1u);

        if (level_width < 8 || level_height < 8) {
            break;
        }

        D3D11_TEXTURE2D_DESC description = source;
        description.Width = level_width;
        description.Height = level_height;
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.SampleDesc.Count = 1;
        description.SampleDesc.Quality = 0;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags =
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        description.CPUAccessFlags = 0;
        description.MiscFlags = 0;
        description.Format = typeless_format(source.Format);

        result = device_->CreateTexture2D(
            &description, nullptr, &textures_[index]);
        if (SUCCEEDED(result)) {
            D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
            view_description.Format = srgb_view_format(source.Format);
            view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            view_description.Texture2D.MostDetailedMip = 0;
            view_description.Texture2D.MipLevels = 1;
            result = device_->CreateShaderResourceView(
                textures_[index],
                &view_description,
                &views_[index]);
        }
        if (SUCCEEDED(result)) {
            D3D11_RENDER_TARGET_VIEW_DESC target_description = {};
            target_description.Format = srgb_view_format(source.Format);
            target_description.ViewDimension =
                D3D11_RTV_DIMENSION_TEXTURE2D;
            target_description.Texture2D.MipSlice = 0;
            result = device_->CreateRenderTargetView(
                textures_[index],
                &target_description,
                &targets_[index]);
        }
        if (FAILED(result)) {
            break;
        }

        widths_[index] = level_width;
        heights_[index] = level_height;
        ++level_count_;
    }

    if (level_count_ < 2) {
        if (!failure_logged_) {
            log_message(
                "Bloom 0.17.0 sem piramide utilizavel (niveis=%u, "
                "0x%08X); mantendo a pilha visual aprovada.",
                level_count_,
                static_cast<unsigned>(result));
            failure_logged_ = true;
        }
        release();
        return false;
    }

    log_message(
        "Bloom 0.17.0 com piramide de %u niveis: %ux%u ate %ux%u.",
        level_count_,
        widths_[0],
        heights_[0],
        widths_[level_count_ - 1],
        heights_[level_count_ - 1]);
    return true;
}

}  // namespace photorealism
