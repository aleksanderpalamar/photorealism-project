#pragma once

#include "easu_constants.hpp"
#include "rcas_constants.hpp"
#include "render_scale.hpp"

#include <d3d11.h>

namespace photorealism {
namespace fsr {

class UpscaleResources {
  public:
    bool ensure(ID3D11Device* device, unsigned width, unsigned height);
    void release();

    bool ready() const { return access_ != nullptr && view_ != nullptr; }
    bool matches(unsigned width, unsigned height) const {
        return ready() && width_ == width && height_ == height;
    }
    ID3D11UnorderedAccessView* access() const { return access_; }
    ID3D11ShaderResourceView* view() const { return view_; }

  private:
    ID3D11Texture2D* texture_ = nullptr;
    ID3D11UnorderedAccessView* access_ = nullptr;
    ID3D11ShaderResourceView* view_ = nullptr;
    unsigned width_ = 0;
    unsigned height_ = 0;
};

}
}
