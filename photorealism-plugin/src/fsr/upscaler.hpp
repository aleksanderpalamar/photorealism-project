#pragma once

#include "../config/settings.hpp"
#include "proxy_target.hpp"
#include "render_scale.hpp"
#include "upscale_pipeline.hpp"

namespace photorealism {
namespace fsr {

class Upscaler {
  public:
    void configure(const Settings& settings);
    void apply_requested();
    void release();

    bool wants_proxy() const { return enabled_; }
    const RenderExtent& extent() const { return extent_; }
    ID3D11ShaderResourceView* source() const { return proxy_.view(); }

    ID3D11Texture2D* ensure_proxy(
        ID3D11Device* device, const D3D11_TEXTURE2D_DESC& back_buffer);

    bool present(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* output,
        unsigned width,
        unsigned height);

  private:
    ProxyTarget proxy_;
    UpscalePipeline pipeline_;
    RenderExtent extent_;
    bool enabled_ = false;
    float scale_ = 1.0f;
    float sharpness_ = 0.0f;
    bool requested_enabled_ = false;
    float requested_scale_ = 1.0f;
};

Upscaler& upscaler();

}
}
