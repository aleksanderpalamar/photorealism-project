#pragma once

#include <d3d11.h>

namespace photorealism {

enum class ConstantSlot : unsigned {
    Visual,
    DepthPreview,
    Temporal,
    Fxaa,
    Occlusion,
    Compose,
    InteriorLight,
    Sharpen,
};

constexpr unsigned kConstantSlotCount = 8;

class PipelineState {
  public:
    bool create(ID3D11Device* device);
    void release();

    ID3D11Buffer* constants(ConstantSlot slot) const {
        return constants_[static_cast<unsigned>(slot)];
    }
    ID3D11Buffer* const* constants_address(ConstantSlot slot) const {
        return &constants_[static_cast<unsigned>(slot)];
    }
    ID3D11SamplerState* linear_sampler() const { return linear_sampler_; }
    ID3D11SamplerState* point_sampler() const { return point_sampler_; }
    ID3D11RasterizerState* rasterizer() const { return rasterizer_; }
    ID3D11DepthStencilState* depth_stencil() const { return depth_stencil_; }
    ID3D11BlendState* opaque_blend() const { return opaque_blend_; }
    ID3D11BlendState* additive_blend() const { return additive_blend_; }

    ID3D11SamplerState* const* linear_sampler_address() const {
        return &linear_sampler_;
    }
    ID3D11SamplerState* const* point_sampler_address() const {
        return &point_sampler_;
    }

  private:
    bool create_constant_buffers(ID3D11Device* device);
    bool create_sampler_states(ID3D11Device* device);
    bool create_raster_states(ID3D11Device* device);
    bool create_opaque_blend_state(ID3D11Device* device);
    void create_additive_blend_state(ID3D11Device* device);

    ID3D11Buffer* constants_[kConstantSlotCount] = {};
    ID3D11SamplerState* linear_sampler_ = nullptr;
    ID3D11SamplerState* point_sampler_ = nullptr;
    ID3D11RasterizerState* rasterizer_ = nullptr;
    ID3D11DepthStencilState* depth_stencil_ = nullptr;
    ID3D11BlendState* opaque_blend_ = nullptr;
    ID3D11BlendState* additive_blend_ = nullptr;
};

}  // namespace photorealism
