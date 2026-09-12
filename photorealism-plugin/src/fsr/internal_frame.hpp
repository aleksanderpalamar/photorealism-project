#pragma once

#include "render_scale.hpp"

#include <d3d11.h>

namespace photorealism {
namespace fsr {

class InternalFrame {
  public:
    bool capture(ID3D11Device* device, ID3D11DeviceContext* context);
    void release();

    ID3D11ShaderResourceView* view() const { return view_; }
    const RenderExtent& extent() const { return extent_; }

  private:
    bool ensure(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& source);

    ID3D11Texture2D* copy_ = nullptr;
    ID3D11ShaderResourceView* view_ = nullptr;
    RenderExtent extent_;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
};

}
}
