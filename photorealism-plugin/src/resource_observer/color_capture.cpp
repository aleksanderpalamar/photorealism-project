#include "color_capture.hpp"

#include "../postprocess/com_utils.hpp"
#include "../runtime.hpp"
#include "../scene/formats.hpp"

namespace photorealism {
namespace observer {

bool ColorCapture::ensure(
    ID3D11DeviceContext* context, const D3D11_TEXTURE2D_DESC& source) {
    const UINT family = scene_formats::resolve_typeless(
        static_cast<unsigned>(source.Format));
    if (copy_ != nullptr && owner_ == context && width_ == source.Width &&
        height_ == source.Height && family_ == family) {
        return true;
    }
    release();

    ID3D11Device* device = nullptr;
    context->GetDevice(&device);
    if (device == nullptr) {
        return false;
    }

    D3D11_TEXTURE2D_DESC description = {};
    description.Width = source.Width;
    description.Height = source.Height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = static_cast<DXGI_FORMAT>(family);
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    HRESULT result = device->CreateTexture2D(&description, nullptr, &copy_);

    D3D11_SHADER_RESOURCE_VIEW_DESC view = {};
    view.Format = static_cast<DXGI_FORMAT>(scene_formats::resolve_unorm(
        static_cast<unsigned>(source.Format)));
    view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = 1;
    if (SUCCEEDED(result)) {
        result = device->CreateShaderResourceView(copy_, &view, &view_);
    }
    device->Release();

    if (FAILED(result)) {
        log_message(
            "FSR nao criou a copia do quadro interno %ux%u: 0x%08X.",
            source.Width,
            source.Height,
            static_cast<unsigned>(result));
        release();
        return false;
    }

    owner_ = context;
    width_ = source.Width;
    height_ = source.Height;
    family_ = family;
    log_message(
        "FSR copia o quadro interno de %ux%u formato=%u no instante em que o "
        "jogo passa para a resolucao de saida.",
        source.Width,
        source.Height,
        static_cast<unsigned>(source.Format));
    return true;
}

bool ColorCapture::copy_from(
    ID3D11DeviceContext* context, ID3D11Texture2D* source) {
    if (context == nullptr || source == nullptr) {
        return false;
    }
    D3D11_TEXTURE2D_DESC described = {};
    source->GetDesc(&described);
    if (!ensure(context, described)) {
        return false;
    }
    context->CopyResource(copy_, source);
    return true;
}

void ColorCapture::release() {
    safe_release(view_);
    safe_release(copy_);
    owner_ = nullptr;
    width_ = 0;
    height_ = 0;
    family_ = 0;
}

}
}
