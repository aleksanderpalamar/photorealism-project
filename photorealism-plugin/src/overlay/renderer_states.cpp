#include "renderer.hpp"

#include "../postprocess/shader_compile.hpp"
#include "../runtime.hpp"

namespace photorealism {
namespace overlay {
namespace {

bool create_vertex_stage(
    CompileFromFileFunction compile,
    ID3D11Device* device,
    ID3D11VertexShader** shader) {
    ID3DBlob* blob = compile_shader_blob(
        compile, overlay_shader_path(), "VSOverlay", "vs_5_0", "overlay-vs");
    if (blob == nullptr) {
        return false;
    }
    const HRESULT result = device->CreateVertexShader(
        blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, shader);
    blob->Release();
    return SUCCEEDED(result) && *shader != nullptr;
}

bool create_pixel_stage(
    CompileFromFileFunction compile,
    ID3D11Device* device,
    ID3D11PixelShader** shader) {
    ID3DBlob* blob = compile_shader_blob(
        compile, overlay_shader_path(), "PSOverlay", "ps_5_0", "overlay-ps");
    if (blob == nullptr) {
        return false;
    }
    const HRESULT result = device->CreatePixelShader(
        blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, shader);
    blob->Release();
    return SUCCEEDED(result) && *shader != nullptr;
}

}

bool Renderer::create_shaders(ID3D11Device* device) {
    CompileFromFileFunction compile = resolve_shader_compiler();
    if (compile == nullptr) {
        log_message("Overlay sem compilador de shader disponivel.");
        return false;
    }
    if (!create_vertex_stage(compile, device, &vertex_shader_)) {
        log_message("Overlay falhou ao criar o vertex shader.");
        return false;
    }
    if (!create_pixel_stage(compile, device, &pixel_shader_)) {
        log_message("Overlay falhou ao criar o pixel shader.");
        return false;
    }
    return true;
}

bool Renderer::create_states(ID3D11Device* device, bool smooth) {
    D3D11_BLEND_DESC blend = {};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device->CreateBlendState(&blend, &blend_))) {
        log_message("Overlay falhou ao criar o blend alpha.");
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depth = {};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if (FAILED(device->CreateDepthStencilState(&depth, &depth_))) {
        return false;
    }

    D3D11_RASTERIZER_DESC rasterizer = {};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;
    if (FAILED(device->CreateRasterizerState(&rasterizer, &rasterizer_))) {
        return false;
    }

    D3D11_SAMPLER_DESC sampler = {};
    sampler.Filter = smooth ? D3D11_FILTER_MIN_MAG_MIP_LINEAR
                            : D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    return SUCCEEDED(device->CreateSamplerState(&sampler, &sampler_));
}

bool Renderer::create_constants(ID3D11Device* device) {
    D3D11_BUFFER_DESC description = {};
    description.ByteWidth = sizeof(OverlayConstants);
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    return SUCCEEDED(device->CreateBuffer(&description, nullptr, &constants_));
}

}
}
