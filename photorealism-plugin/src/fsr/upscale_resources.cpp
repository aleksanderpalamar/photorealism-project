#include "upscale_resources.hpp"

#include "../postprocess/com_utils.hpp"
#include "../runtime.hpp"

namespace photorealism {
namespace fsr {
namespace {

constexpr DXGI_FORMAT kUpscaleFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

}

bool UpscaleResources::ensure(
    ID3D11Device* device, unsigned width, unsigned height) {
    if (device == nullptr || width == 0 || height == 0) {
        return false;
    }
    if (ready() && width_ == width && height_ == height) {
        return true;
    }
    release();

    D3D11_TEXTURE2D_DESC description = {};
    description.Width = width;
    description.Height = height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = kUpscaleFormat;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags =
        D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    const HRESULT created =
        device->CreateTexture2D(&description, nullptr, &texture_);
    if (FAILED(created)) {
        log_message(
            "FSR nao criou a textura de saida %ux%u: 0x%08X.",
            width,
            height,
            static_cast<unsigned>(created));
        return false;
    }

    const HRESULT accessed = device->CreateUnorderedAccessView(
        texture_, nullptr, &access_);
    if (FAILED(accessed)) {
        log_message(
            "FSR nao criou a UAV da saida: 0x%08X. Sem escrita de compute o "
            "upscale nao roda.",
            static_cast<unsigned>(accessed));
        release();
        return false;
    }

    const HRESULT viewed =
        device->CreateShaderResourceView(texture_, nullptr, &view_);
    if (FAILED(viewed)) {
        log_message("FSR nao criou a view da saida do EASU.");
        release();
        return false;
    }

    width_ = width;
    height_ = height;
    return true;
}

void UpscaleResources::release() {
    safe_release(view_);
    safe_release(access_);
    safe_release(texture_);
    width_ = 0;
    height_ = 0;
}

}
}
