#pragma once

#include "render_scale.hpp"

#include <d3d11.h>

namespace photorealism {
namespace fsr {

class InternalFrame {
  public:
    bool acquire();
    void release();

    ID3D11ShaderResourceView* view() const { return view_; }
    const RenderExtent& extent() const { return extent_; }

  private:
    ID3D11ShaderResourceView* view_ = nullptr;
    RenderExtent extent_;
};

}
}
