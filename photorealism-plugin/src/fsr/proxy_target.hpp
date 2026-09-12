#pragma once

#include "render_scale.hpp"

#include <d3d11.h>

namespace photorealism {
namespace fsr {

class ProxyTarget {
  public:
    bool ensure(
        ID3D11Device* device,
        const D3D11_TEXTURE2D_DESC& back_buffer,
        const RenderExtent& internal);
    void release();

    bool ready() const { return texture_ != nullptr; }
    ID3D11Texture2D* texture() const { return texture_; }
    ID3D11ShaderResourceView* view() const { return view_; }
    const RenderExtent& extent() const { return extent_; }

  private:
    bool matches(
        const D3D11_TEXTURE2D_DESC& back_buffer,
        const RenderExtent& internal) const;

    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* view_ = nullptr;
    RenderExtent extent_;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    UINT sample_count_ = 0;
};

}
}
