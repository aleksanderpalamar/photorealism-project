#include "frame_resources.hpp"

#include "../runtime.hpp"
#include "com_utils.hpp"
#include "format_utils.hpp"

namespace photorealism {
namespace {

void release_intermediate(IntermediateTarget* intermediate) {
    safe_release(intermediate->target);
    safe_release(intermediate->raw_view);
    safe_release(intermediate->view);
    safe_release(intermediate->texture);
}

bool intermediate_complete(const IntermediateTarget& intermediate) {
    return intermediate.texture != nullptr && intermediate.view != nullptr &&
           intermediate.raw_view != nullptr && intermediate.target != nullptr;
}

}

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

HRESULT FrameResources::create_view(
    ID3D11Texture2D* texture, DXGI_FORMAT format,
    ID3D11ShaderResourceView** view) {
    D3D11_SHADER_RESOURCE_VIEW_DESC description = {};
    description.Format = format;
    description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    description.Texture2D.MipLevels = 1;
    return device_->CreateShaderResourceView(texture, &description, view);
}

HRESULT FrameResources::create_intermediate(
    const D3D11_TEXTURE2D_DESC& source, IntermediateTarget* intermediate) {
    const D3D11_TEXTURE2D_DESC description = intermediate_description(
        source, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
    HRESULT result =
        device_->CreateTexture2D(&description, nullptr, &intermediate->texture);
    result = SUCCEEDED(result)
                 ? create_view(intermediate->texture,
                               srgb_view_format(source.Format), &intermediate->view)
                 : result;
    result = SUCCEEDED(result)
                 ? create_view(intermediate->texture,
                               unorm_view_format(source.Format),
                               &intermediate->raw_view)
                 : result;
    D3D11_RENDER_TARGET_VIEW_DESC target_description = {};
    target_description.Format = srgb_view_format(source.Format);
    target_description.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    return SUCCEEDED(result)
               ? device_->CreateRenderTargetView(
                     intermediate->texture, &target_description,
                     &intermediate->target)
               : result;
}

bool FrameResources::create_scene_texture(const D3D11_TEXTURE2D_DESC& source) {
    D3D11_TEXTURE2D_DESC description =
        intermediate_description(source, D3D11_BIND_SHADER_RESOURCE);
    HRESULT result =
        device_->CreateTexture2D(&description, nullptr, &scene_texture_);
    result = SUCCEEDED(result)
                 ? create_view(scene_texture_, srgb_view_format(source.Format),
                               &scene_view_)
                 : result;
    scene_needs_srgb_decode_ = false;
    if (SUCCEEDED(result)) {
        return true;
    }
    safe_release(scene_view_);
    safe_release(scene_texture_);

    description.Format = source.Format;
    result = device_->CreateTexture2D(&description, nullptr, &scene_texture_);
    result = SUCCEEDED(result)
                 ? device_->CreateShaderResourceView(scene_texture_, nullptr, &scene_view_)
                 : result;
    scene_needs_srgb_decode_ = is_unorm_format(source.Format);
    if (!input_fallback_logged_) {
        log_message("SRV sRGB indisponivel; usando decodificacao manual na entrada.");
        input_fallback_logged_ = true;
    }
    if (SUCCEEDED(result)) {
        return true;
    }
    log_message(
        "Falha ao criar recursos intermediarios: 0x%08X.",
        static_cast<unsigned>(result));
    safe_release(scene_view_);
    safe_release(scene_texture_);
    return false;
}

void FrameResources::create_named_intermediate(
    const D3D11_TEXTURE2D_DESC& source, IntermediateTarget* intermediate,
    const char* name) {
    const HRESULT result = create_intermediate(source, intermediate);
    if (SUCCEEDED(result) && intermediate_complete(*intermediate)) {
        return;
    }
    release_intermediate(intermediate);
    if (!intermediate_failure_logged_) {
        log_message(
            "Sem textura intermediaria %s: 0x%08X; efeitos encadeados ficam "
            "desligados e o passe visual continua.",
            name, static_cast<unsigned>(result));
        intermediate_failure_logged_ = true;
    }
}

void FrameResources::release_scene_textures() {
    release_intermediate(&second_);
    release_intermediate(&first_);
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
    create_named_intermediate(source, &first_, "primeira");
    create_named_intermediate(source, &second_, "segunda");
    width_ = source.Width;
    height_ = source.Height;
    format_ = source.Format;
    return true;
}

}
