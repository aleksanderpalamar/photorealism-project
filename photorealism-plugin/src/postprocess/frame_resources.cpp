#include "frame_resources.hpp"

#include "../runtime.hpp"
#include "com_utils.hpp"
#include "format_utils.hpp"

namespace photorealism {

void FrameResources::attach(ID3D11Device* device) {
    device_ = device;
}

void FrameResources::release() {
    release_scene_textures();
    width_ = 0;
    height_ = 0;
    format_ = DXGI_FORMAT_UNKNOWN;
    scene_needs_srgb_decode_ = false;
}

void FrameResources::release_intermediate_target(FrameResources::IntermediateTarget* intermediate) {
    safe_release(intermediate->target);
    safe_release(intermediate->view);
    safe_release(intermediate->texture);
}

D3D11_TEXTURE2D_DESC FrameResources::intermediate_description(
    const D3D11_TEXTURE2D_DESC& source, UINT bind_flags) const {
    D3D11_TEXTURE2D_DESC description = source;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = bind_flags;
    description.CPUAccessFlags = 0;
    description.MiscFlags = 0;
    description.Format = typeless_format(source.Format);
    return description;
}

HRESULT FrameResources::create_srgb_view(
    ID3D11Texture2D* texture,
    const D3D11_TEXTURE2D_DESC& source,
    ID3D11ShaderResourceView** view) {
    D3D11_SHADER_RESOURCE_VIEW_DESC description = {};
    description.Format = srgb_view_format(source.Format);
    description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    description.Texture2D.MostDetailedMip = 0;
    description.Texture2D.MipLevels = source.MipLevels;
    return device_->CreateShaderResourceView(texture, &description, view);
}

HRESULT FrameResources::create_srgb_target(
    ID3D11Texture2D* texture,
    const D3D11_TEXTURE2D_DESC& source,
    ID3D11RenderTargetView** target) {
    D3D11_RENDER_TARGET_VIEW_DESC description = {};
    description.Format = srgb_view_format(source.Format);
    description.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    description.Texture2D.MipSlice = 0;
    return device_->CreateRenderTargetView(texture, &description, target);
}

HRESULT FrameResources::create_intermediate_target(
    const D3D11_TEXTURE2D_DESC& source, FrameResources::IntermediateTarget* intermediate) {
    const D3D11_TEXTURE2D_DESC description = intermediate_description(
        source, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
    HRESULT result = device_->CreateTexture2D(
        &description, nullptr, &intermediate->texture);
    if (FAILED(result) || intermediate->texture == nullptr) {
        return FAILED(result) ? result : E_FAIL;
    }
    result = create_srgb_view(
        intermediate->texture, source, &intermediate->view);
    if (FAILED(result)) {
        return result;
    }
    return create_srgb_target(
        intermediate->texture, source, &intermediate->target);
}

bool FrameResources::intermediate_is_complete(
    HRESULT result, const FrameResources::IntermediateTarget& intermediate) {
    return SUCCEEDED(result) && intermediate.texture != nullptr &&
           intermediate.view != nullptr && intermediate.target != nullptr;
}

bool FrameResources::create_scene_texture(const D3D11_TEXTURE2D_DESC& source) {
    D3D11_TEXTURE2D_DESC description =
        intermediate_description(source, D3D11_BIND_SHADER_RESOURCE);
    HRESULT result = device_->CreateTexture2D(
        &description, nullptr, &scene_texture_);
    if (SUCCEEDED(result) && scene_texture_ != nullptr) {
        result = create_srgb_view(scene_texture_, source, &scene_view_);
    }

    scene_needs_srgb_decode_ = false;
    if (FAILED(result) || scene_texture_ == nullptr ||
        scene_view_ == nullptr) {
        safe_release(scene_view_);
        safe_release(scene_texture_);

        description.Format = source.Format;
        result = device_->CreateTexture2D(
            &description, nullptr, &scene_texture_);
        if (SUCCEEDED(result) && scene_texture_ != nullptr) {
            result = device_->CreateShaderResourceView(
                scene_texture_, nullptr, &scene_view_);
        }
        scene_needs_srgb_decode_ = is_unorm_format(source.Format);
        if (!input_fallback_logged_) {
            log_message(
                "SRV sRGB indisponivel; usando decodificacao manual na entrada.");
            input_fallback_logged_ = true;
        }
    }

    if (SUCCEEDED(result) && scene_texture_ != nullptr &&
        scene_view_ != nullptr) {
        return true;
    }
    log_message(
        "Falha ao criar recursos intermediarios: 0x%08X.",
        static_cast<unsigned>(result));
    safe_release(scene_view_);
    safe_release(scene_texture_);
    return false;
}

void FrameResources::create_visual_target(const D3D11_TEXTURE2D_DESC& source) {
    FrameResources::IntermediateTarget created = {};
    const HRESULT result = create_intermediate_target(source, &created);
    if (intermediate_is_complete(result, created)) {
        visual_texture_ = created.texture;
        visual_view_ = created.view;
        visual_target_ = created.target;
        return;
    }
    if (!visual_failure_logged_) {
        log_message(
            "SSAO 0.9.1 sem textura intermediaria: 0x%08X; "
            "mantendo o passe visual normal.",
            static_cast<unsigned>(result));
        visual_failure_logged_ = true;
    }
    release_intermediate_target(&created);
}

void FrameResources::create_spatial_target(const D3D11_TEXTURE2D_DESC& source) {
    FrameResources::IntermediateTarget created = {};
    const HRESULT result = create_intermediate_target(source, &created);
    if (intermediate_is_complete(result, created)) {
        spatial_texture_ = created.texture;
        spatial_view_ = created.view;
        spatial_target_ = created.target;
        return;
    }
    if (!spatial_failure_logged_) {
        log_message(
            "Temporal 0.10.0 sem textura espacial: 0x%08X; "
            "mantendo a pilha visual/SSAO anterior.",
            static_cast<unsigned>(result));
        spatial_failure_logged_ = true;
    }
    release_intermediate_target(&created);
}

void FrameResources::release_scene_textures() {
    safe_release(spatial_target_);
    safe_release(spatial_view_);
    safe_release(spatial_texture_);
    safe_release(visual_target_);
    safe_release(visual_view_);
    safe_release(visual_texture_);
    safe_release(scene_view_);
    safe_release(scene_texture_);
}

bool FrameResources::matches(const D3D11_TEXTURE2D_DESC& source) const {
    return scene_texture_ != nullptr && width_ == source.Width &&
           height_ == source.Height && format_ == source.Format;
}

bool FrameResources::create(const D3D11_TEXTURE2D_DESC& source) {
    release_scene_textures();
    if (!create_scene_texture(source)) {
        return false;
    }

    create_visual_target(source);
    if (visual_target_ != nullptr) {
        create_spatial_target(source);
    }

    width_ = source.Width;
    height_ = source.Height;
    format_ = source.Format;
    log_message(
        "Recursos de frame criados: %ux%u format=%u "
        "ssao_intermediate=%s temporal_spatial=%s.",
        width_,
        height_,
        static_cast<unsigned>(format_),
        visual_target_ != nullptr ? "ok" : "indisponivel",
        spatial_target_ != nullptr ? "ok" : "indisponivel");
    return true;
}

}  // namespace photorealism
