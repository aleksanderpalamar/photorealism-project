#pragma once

#include <d3d11.h>

namespace photorealism {
namespace fsr {

class GrainTexture {
  public:
    bool create(ID3D11Device* device);
    void release();

    bool ready() const { return view_ != nullptr; }
    ID3D11ShaderResourceView* view() const { return view_; }

  private:
    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* view_ = nullptr;
};

}
}
