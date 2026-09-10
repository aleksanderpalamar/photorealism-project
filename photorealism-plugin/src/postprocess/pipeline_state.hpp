#pragma once

#include <d3d11.h>

namespace photorealism {

class PipelineState {
  public:
    bool create(ID3D11Device* device);
    void release();

    ID3D11Buffer* visual_constants() const { return visual_constants_; }
    ID3D11Buffer* depth_constants() const { return depth_constants_; }
    ID3D11Buffer* ssao_constants() const { return ssao_constants_; }
    ID3D11Buffer* temporal_constants() const { return temporal_constants_; }
    ID3D11SamplerState* linear_sampler() const { return linear_sampler_; }
    ID3D11SamplerState* point_sampler() const { return point_sampler_; }
    ID3D11RasterizerState* rasterizer() const { return rasterizer_; }
    ID3D11DepthStencilState* depth_stencil() const { return depth_stencil_; }
    ID3D11BlendState* opaque_blend() const { return opaque_blend_; }
    ID3D11BlendState* additive_blend() const { return additive_blend_; }

    ID3D11Buffer* const* visual_constants_address() const {
        return &visual_constants_;
    }
    ID3D11Buffer* const* depth_constants_address() const {
        return &depth_constants_;
    }
    ID3D11Buffer* const* ssao_constants_address() const {
        return &ssao_constants_;
    }
    ID3D11Buffer* const* temporal_constants_address() const {
        return &temporal_constants_;
    }
    ID3D11SamplerState* const* linear_sampler_address() const {
        return &linear_sampler_;
    }
    ID3D11SamplerState* const* point_sampler_address() const {
        return &point_sampler_;
    }

  private:
    struct ConstantBufferSlot {
        UINT byte_width;
        ID3D11Buffer* PipelineState::*member;
        const char* description;
    };

    bool create_constant_buffers(ID3D11Device* device);
    bool create_sampler_states(ID3D11Device* device);
    bool create_raster_states(ID3D11Device* device);
    bool create_opaque_blend_state(ID3D11Device* device);
    void create_additive_blend_state(ID3D11Device* device);

    ID3D11Buffer* visual_constants_ = nullptr;
    ID3D11Buffer* depth_constants_ = nullptr;
    ID3D11Buffer* ssao_constants_ = nullptr;
    ID3D11Buffer* temporal_constants_ = nullptr;
    ID3D11SamplerState* linear_sampler_ = nullptr;
    ID3D11SamplerState* point_sampler_ = nullptr;
    ID3D11RasterizerState* rasterizer_ = nullptr;
    ID3D11DepthStencilState* depth_stencil_ = nullptr;
    ID3D11BlendState* opaque_blend_ = nullptr;
    ID3D11BlendState* additive_blend_ = nullptr;
};

}  // namespace photorealism
