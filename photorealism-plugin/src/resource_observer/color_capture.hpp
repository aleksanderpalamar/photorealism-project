#pragma once

#include <d3d11.h>

namespace photorealism {
namespace observer {

class ColorCapture {
  public:
    bool copy_from(ID3D11DeviceContext* context, ID3D11Texture2D* source);
    void release();

    ID3D11ShaderResourceView* view() const { return view_; }
    UINT width() const { return width_; }
    UINT height() const { return height_; }

  private:
    bool ensure(ID3D11DeviceContext* context, const D3D11_TEXTURE2D_DESC& source);

    ID3D11Texture2D* copy_ = nullptr;
    ID3D11ShaderResourceView* view_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;
    UINT family_ = 0;
};

}
}
