#include "temporal_history.hpp"

#include "../runtime.hpp"
#include "com_utils.hpp"
#include "format_utils.hpp"

namespace photorealism {

void TemporalHistory::attach(
    ID3D11Device* device, ID3D11DeviceContext* context) {
    device_ = device;
    context_ = context;
}

void TemporalHistory::store(
    ID3D11Texture2D* back_buffer, ID3D11Texture2D* depth_copy_texture) {
    context_->CopyResource(color_texture_, back_buffer);
    context_->CopyResource(depth_texture_, depth_copy_texture);
}

bool TemporalHistory::invalidate() {
    if (!valid_) {
        return false;
    }
    valid_ = false;
    return true;
}

bool TemporalHistory::ensure(
    const D3D11_TEXTURE2D_DESC& frame_description,
    const D3D11_TEXTURE2D_DESC& depth_description,
    std::uint64_t generation,
    bool temporal_shader_available,
    ID3D11Texture2D* depth_copy_texture) {
    if (device_ == nullptr || depth_copy_texture == nullptr ||
        !temporal_shader_available || generation == 0) {
        return false;
    }
    if (color_texture_ != nullptr &&
        color_view_ != nullptr &&
        depth_texture_ != nullptr &&
        depth_view_ != nullptr &&
        generation_ == generation &&
        depth_width_ == depth_description.Width &&
        depth_height_ == depth_description.Height) {
        return true;
    }

    release();

    D3D11_TEXTURE2D_DESC color_description = frame_description;
    color_description.Usage = D3D11_USAGE_DEFAULT;
    color_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    color_description.CPUAccessFlags = 0;
    color_description.MiscFlags = 0;
    color_description.Format = typeless_format(frame_description.Format);
    HRESULT result = device_->CreateTexture2D(
        &color_description, nullptr, &color_texture_);
    if (SUCCEEDED(result) && color_texture_ != nullptr) {
        D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
        view_description.Format = srgb_view_format(frame_description.Format);
        view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        view_description.Texture2D.MostDetailedMip = 0;
        view_description.Texture2D.MipLevels = frame_description.MipLevels;
        result = device_->CreateShaderResourceView(
            color_texture_,
            &view_description,
            &color_view_);
    }

    DXGI_FORMAT depth_resource_format = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT depth_view_format = DXGI_FORMAT_UNKNOWN;
    if (SUCCEEDED(result) &&
        depth_copy_formats(
            depth_description.Format,
            &depth_resource_format,
            &depth_view_format)) {
        D3D11_TEXTURE2D_DESC history_depth_description = depth_description;
        history_depth_description.Format = depth_resource_format;
        history_depth_description.Usage = D3D11_USAGE_DEFAULT;
        history_depth_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        history_depth_description.CPUAccessFlags = 0;
        history_depth_description.MiscFlags = 0;
        result = device_->CreateTexture2D(
            &history_depth_description,
            nullptr,
            &depth_texture_);
        if (SUCCEEDED(result) &&
            depth_texture_ != nullptr) {
            D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
            view_description.Format = depth_view_format;
            view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            view_description.Texture2D.MostDetailedMip = 0;
            view_description.Texture2D.MipLevels = 1;
            result = device_->CreateShaderResourceView(
                depth_texture_,
                &view_description,
                &depth_view_);
        }
    } else if (SUCCEEDED(result)) {
        result = E_INVALIDARG;
    }

    if (FAILED(result) || color_texture_ == nullptr ||
        color_view_ == nullptr ||
        depth_texture_ == nullptr ||
        depth_view_ == nullptr) {
        if (!failure_logged_) {
            log_message(
                "Falha ao criar historico temporal 0.10.0: "
                "result=0x%08X color=%ux%u depth=%ux%u format=%u.",
                static_cast<unsigned>(result),
                frame_description.Width,
                frame_description.Height,
                depth_description.Width,
                depth_description.Height,
                static_cast<unsigned>(depth_description.Format));
            failure_logged_ = true;
        }
        release();
        return false;
    }

    generation_ = generation;
    depth_width_ = depth_description.Width;
    depth_height_ = depth_description.Height;
    valid_ = false;
    failure_logged_ = false;
    log_message(
        "Recursos temporais 0.10.0 criados: color=%ux%u depth=%ux%u "
        "generation=%llu.",
        frame_description.Width,
        frame_description.Height,
        depth_description.Width,
        depth_description.Height,
        static_cast<unsigned long long>(generation));
    return true;
}

void TemporalHistory::release() {
    safe_release(depth_view_);
    safe_release(depth_texture_);
    safe_release(color_view_);
    safe_release(color_texture_);
    generation_ = 0;
    depth_width_ = 0;
    depth_height_ = 0;
    valid_ = false;
}

}  // namespace photorealism
