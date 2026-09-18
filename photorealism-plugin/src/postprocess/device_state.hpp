#pragma once

#include <d3d11_1.h>

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
    ID3D11ShaderResourceView* vertex_resource = nullptr;
    ID3D11ShaderResourceView* pixel_resources[4] = {};
    ID3D11SamplerState* pixel_samplers[2] = {};
    ID3D11Buffer* pixel_constant_buffer = nullptr;
    ID3D11DeviceContext1* pixel_context1 = nullptr;
    UINT pixel_first_constant = 0;
    UINT pixel_constant_count = 0;
    ID3D11ComputeShader* compute_shader = nullptr;
    ID3D11ShaderResourceView* compute_resource = nullptr;
    ID3D11UnorderedAccessView* compute_access = nullptr;
    ID3D11Buffer* compute_constant_buffer = nullptr;
};

void capture_state(ID3D11DeviceContext* context, SavedState* state);
void restore_state(ID3D11DeviceContext* context, SavedState* state);

}  // namespace photorealism
