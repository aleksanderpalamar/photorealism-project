#include "proxy_target.hpp"

#include "../postprocess/com_utils.hpp"
#include "../runtime.hpp"

namespace photorealism {
namespace fsr {

bool ProxyTarget::matches(
    const D3D11_TEXTURE2D_DESC& back_buffer,
    const RenderExtent& internal) const {
    return texture_ != nullptr && extent_.width == internal.width &&
           extent_.height == internal.height &&
           format_ == back_buffer.Format &&
           sample_count_ == back_buffer.SampleDesc.Count;
}

bool ProxyTarget::ensure(
    ID3D11Device* device,
    const D3D11_TEXTURE2D_DESC& back_buffer,
    const RenderExtent& internal) {
    if (device == nullptr || internal.width == 0 || internal.height == 0) {
        return false;
    }
    if (matches(back_buffer, internal)) {
        return true;
    }
    release();

    D3D11_TEXTURE2D_DESC description = back_buffer;
    description.Width = internal.width;
    description.Height = internal.height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags =
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    description.CPUAccessFlags = 0;
    description.MiscFlags = 0;

    const HRESULT created =
        device->CreateTexture2D(&description, nullptr, &texture_);
    if (FAILED(created)) {
        log_message(
            "FSR nao criou a textura interna %ux%u formato %u: 0x%08X.",
            internal.width,
            internal.height,
            static_cast<unsigned>(back_buffer.Format),
            static_cast<unsigned>(created));
        return false;
    }

    const HRESULT viewed =
        device->CreateShaderResourceView(texture_, nullptr, &view_);
    if (FAILED(viewed)) {
        log_message(
            "FSR nao criou a view da textura interna: 0x%08X.",
            static_cast<unsigned>(viewed));
        release();
        return false;
    }

    extent_ = internal;
    format_ = back_buffer.Format;
    sample_count_ = back_buffer.SampleDesc.Count;
    log_message(
        "FSR entregara ao jogo uma textura interna de %ux%u no lugar do "
        "backbuffer.",
        internal.width,
        internal.height);
    return true;
}

void ProxyTarget::release() {
    safe_release(view_);
    safe_release(texture_);
    extent_ = RenderExtent{};
    format_ = DXGI_FORMAT_UNKNOWN;
    sample_count_ = 0;
}

}
}
