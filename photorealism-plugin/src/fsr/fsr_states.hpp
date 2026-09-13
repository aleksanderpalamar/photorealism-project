#pragma once

#include <d3d11.h>

namespace photorealism {
namespace fsr {

class FsrStates {
  public:
    bool create(ID3D11Device* device);
    void release();

    bool ready() const {
        return blend_ != nullptr && depth_ != nullptr && rasterizer_ != nullptr;
    }

    void bind(ID3D11DeviceContext* context) const;

  private:
    ID3D11BlendState* blend_ = nullptr;
    ID3D11DepthStencilState* depth_ = nullptr;
    ID3D11RasterizerState* rasterizer_ = nullptr;
};

}
}
