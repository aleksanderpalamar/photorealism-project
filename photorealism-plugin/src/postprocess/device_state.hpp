#pragma once

#include <d3d11.h>

namespace photorealism {

struct SavedState {
    ID3D11RenderTargetView* render_targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* depth_target = nullptr;
    ID3D11BlendState* blend_state = nullptr;
    FLOAT blend_factor[4] = {};
    UINT sample_mask = 0;
    ID3D11DepthStencilState* depth_state = nullptr;
    UINT stencil_reference = 0;
    ID3D11RasterizerState* rasterizer_state = nullptr;
    D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    UINT viewport_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_RECT scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    UINT scissor_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    ID3D11InputLayout* input_layout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11VertexShader* vertex_shader = nullptr;
    ID3D11PixelShader* pixel_shader = nullptr;
    ID3D11GeometryShader* geometry_shader = nullptr;
    ID3D11HullShader* hull_shader = nullptr;
    ID3D11DomainShader* domain_shader = nullptr;
    ID3D11ShaderResourceView* pixel_resources[4] = {};
    ID3D11SamplerState* pixel_samplers[2] = {};
    ID3D11Buffer* pixel_constant_buffer = nullptr;
};

void capture_state(ID3D11DeviceContext* context, SavedState* state);
void restore_state(ID3D11DeviceContext* context, SavedState* state);

}  // namespace photorealism
