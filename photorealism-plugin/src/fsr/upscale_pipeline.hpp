#pragma once

#include "fsr_shaders.hpp"
#include "render_scale.hpp"
#include "upscale_resources.hpp"

namespace photorealism {
namespace fsr {

class UpscalePipeline {
  public:
    bool ensure(ID3D11Device* device, unsigned width, unsigned height);
    void release();

    bool ready() const { return shaders_.ready() && resources_.ready(); }

    bool run(
        ID3D11DeviceContext* context,
        ID3D11ShaderResourceView* source,
        const RenderExtent& internal,
        ID3D11RenderTargetView* output,
        unsigned width,
        unsigned height,
        float sharpness);

  private:
    void dispatch_easu(
        ID3D11DeviceContext* context,
        ID3D11ShaderResourceView* source,
        const RenderExtent& internal,
        unsigned width,
        unsigned height);
    void draw_rcas(
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* output,
        unsigned width,
        unsigned height,
        float sharpness);

    FsrShaders shaders_;
    UpscaleResources resources_;
    ID3D11Device* device_ = nullptr;
};

}
}
