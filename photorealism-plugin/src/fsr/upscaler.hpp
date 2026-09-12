#pragma once

#include "../config/settings.hpp"
#include "render_scale.hpp"
#include "upscale_pipeline.hpp"

namespace photorealism {
namespace fsr {

class Upscaler {
  public:
    void configure(const Settings& settings);
    void release();

    bool wants_proxy() const { return enabled_; }
    bool has_internal_frame() const { return internal_frame_ != nullptr; }
    void set_internal_frame(ID3D11ShaderResourceView* frame) { internal_frame_ = frame; }
    const char* status() const;
    const RenderExtent& extent() const { return extent_; }


    bool present(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* output,
        unsigned width,
        unsigned height);

  private:
    UpscalePipeline pipeline_;
    RenderExtent extent_;
    bool enabled_ = false;
    ID3D11ShaderResourceView* internal_frame_ = nullptr;
    float scale_ = 1.0f;
    float sharpness_ = 0.0f;
};

Upscaler& upscaler();

}
}
