#pragma once

#include "fsr_shaders.hpp"
#include "fsr_states.hpp"
#include "grain_texture.hpp"
#include "render_scale.hpp"
#include "upscale_resources.hpp"

namespace photorealism {
namespace fsr {

struct OutputFinish {
    float sharpness = 0.0f;
    float grain_amount = 0.0f;
    unsigned grain_frame = 0;
    bool output_is_srgb_view = false;
};

class UpscalePipeline {
  public:
    bool ensure(ID3D11Device* device, unsigned width, unsigned height);
    void release();

    bool ready() const {
        return shaders_.ready() && states_.ready() && grain_.ready() &&
               resources_.ready();
    }
    bool ready_for(unsigned width, unsigned height) const {
        return shaders_.ready() && states_.ready() && grain_.ready() &&
               resources_.matches(width, height);
    }

    bool run(
        ID3D11DeviceContext* context,
        ID3D11ShaderResourceView* source,
        const RenderExtent& internal,
        ID3D11RenderTargetView* output,
        unsigned width,
        unsigned height,
        const OutputFinish& finish);

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
        const OutputFinish& finish);

    FsrShaders shaders_;
    FsrStates states_;
    GrainTexture grain_;
    UpscaleResources resources_;
    ID3D11Device* device_ = nullptr;
};

}
}
