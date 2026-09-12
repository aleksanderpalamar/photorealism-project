#include "internal_frame.hpp"

#include "../postprocess/com_utils.hpp"
#include "../resource_observer/color_observation.hpp"
#include "../runtime.hpp"

namespace photorealism {
namespace fsr {

bool InternalFrame::ensure(
    ID3D11Device* device, const D3D11_TEXTURE2D_DESC& source) {
    if (copy_ != nullptr && extent_.width == source.Width &&
        extent_.height == source.Height && format_ == source.Format) {
        return true;
    }
    release();

    D3D11_TEXTURE2D_DESC description = {};
    description.Width = source.Width;
    description.Height = source.Height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = source.Format;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    const HRESULT created =
        device->CreateTexture2D(&description, nullptr, &copy_);
    if (FAILED(created)) {
        log_message(
            "FSR nao criou a copia do quadro interno %ux%u: 0x%08X.",
            source.Width,
            source.Height,
            static_cast<unsigned>(created));
        return false;
    }
    if (FAILED(device->CreateShaderResourceView(copy_, nullptr, &view_))) {
        log_message("FSR nao criou a view da copia do quadro interno.");
        release();
        return false;
    }

    extent_.width = source.Width;
    extent_.height = source.Height;
    format_ = source.Format;
    log_message(
        "FSR copiara o quadro interno de %ux%u a cada apresentacao.",
        source.Width,
        source.Height);
    return true;
}

bool InternalFrame::capture(
    ID3D11Device* device, ID3D11DeviceContext* context) {
    if (device == nullptr || context == nullptr) {
        return false;
    }

    ID3D11Texture2D* source = nullptr;
    D3D11_TEXTURE2D_DESC described = {};
    if (!acquire_color_candidate(&source, &described)) {
        return false;
    }
    if (!ensure(device, described)) {
        safe_release(source);
        return false;
    }

    context->CopyResource(copy_, source);
    safe_release(source);
    return true;
}

void InternalFrame::release() {
    safe_release(view_);
    safe_release(copy_);
    extent_ = RenderExtent{};
    format_ = DXGI_FORMAT_UNKNOWN;
}

}
}
