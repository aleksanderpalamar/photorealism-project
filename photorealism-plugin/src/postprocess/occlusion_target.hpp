#pragma once

#include <d3d11.h>

namespace photorealism {

class OcclusionTarget {
  public:
    void attach(ID3D11Device* device);
    void release();
    bool ensure(UINT width, UINT height);

    ID3D11ShaderResourceView* view() const { return view_; }
    ID3D11RenderTargetView* target() const { return target_; }
    UINT width() const { return width_; }
    UINT height() const { return height_; }

  private:
    ID3D11Device* device_ = nullptr;
    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* view_ = nullptr;
    ID3D11RenderTargetView* target_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;
    bool failure_logged_ = false;
};

}
