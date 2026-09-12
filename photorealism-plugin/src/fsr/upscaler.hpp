#pragma once

#include "../config/settings.hpp"
#include "internal_frame.hpp"
#include "render_scale.hpp"
#include "upscale_pipeline.hpp"

namespace photorealism {
namespace fsr {

class Upscaler {
  public:
    void configure(const Settings& settings);
    void release();

    bool wants_proxy() const { return enabled_; }
    const char* status() const;
    const RenderExtent& extent() const { return extent_; }

    bool present(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* output,
        unsigned width,
        unsigned height,
        bool output_is_srgb_view);

  private:
    bool reconstruct(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* output,
        unsigned width,
        unsigned height,
        bool output_is_srgb_view);

    UpscalePipeline pipeline_;
    InternalFrame internal_;
    RenderExtent extent_;
    bool enabled_ = false;
    bool reconstructing_ = false;
    bool capture_scale_known_ = false;
    float capture_scale_ = 1.0f;
    float scale_ = 1.0f;
    float sharpness_ = 0.0f;
};

Upscaler& upscaler();

}
}
