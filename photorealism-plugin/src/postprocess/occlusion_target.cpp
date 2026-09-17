#include "occlusion_target.hpp"

#include "../runtime.hpp"
#include "com_utils.hpp"

namespace photorealism {

void OcclusionTarget::attach(ID3D11Device* device) {
    device_ = device;
}

void OcclusionTarget::release() {
    safe_release(target_);
    safe_release(view_);
    safe_release(texture_);
    width_ = 0;
    height_ = 0;
}

bool OcclusionTarget::ensure(UINT width, UINT height) {
    if (texture_ != nullptr && width_ == width && height_ == height) {
        return true;
    }
    release();
    if (device_ == nullptr || width == 0 || height == 0) {
        return false;
    }

    D3D11_TEXTURE2D_DESC description = {};
    description.Width = width;
    description.Height = height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    HRESULT result = device_->CreateTexture2D(&description, nullptr, &texture_);
    result = SUCCEEDED(result)
                 ? device_->CreateShaderResourceView(texture_, nullptr, &view_)
                 : result;
    result = SUCCEEDED(result)
                 ? device_->CreateRenderTargetView(texture_, nullptr, &target_)
                 : result;
    if (SUCCEEDED(result)) {
        width_ = width;
        height_ = height;
        return true;
    }
    release();
    if (!failure_logged_) {
        log_message(
            "SSAO sem alvo de oclusao %ux%u: 0x%08X.", width, height,
            static_cast<unsigned>(result));
        failure_logged_ = true;
    }
    return false;
}

}
