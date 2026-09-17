#include "pipeline_state.hpp"

#include "../runtime.hpp"
#include "../fsr/rcas_constants.hpp"
#include "com_utils.hpp"
#include "effect_constants.hpp"
#include "shader_constants.hpp"

#include <cfloat>

namespace photorealism {

bool PipelineState::create(ID3D11Device* device) {
    if (!create_constant_buffers(device) || !create_sampler_states(device) ||
        !create_raster_states(device) || !create_opaque_blend_state(device)) {
        return false;
    }
    create_additive_blend_state(device);
    return true;
}

void PipelineState::release() {
    for (ID3D11Buffer*& buffer : constants_) {
        safe_release(buffer);
    }
    safe_release(linear_sampler_);
    safe_release(point_sampler_);
    safe_release(rasterizer_);
    safe_release(depth_stencil_);
    safe_release(opaque_blend_);
    safe_release(additive_blend_);
}

bool PipelineState::create_constant_buffers(ID3D11Device* device) {
    constexpr UINT kByteWidths[kConstantSlotCount] = {
        sizeof(ShaderConstants),
        sizeof(DepthPreviewConstants),
        sizeof(TemporalConstants),
        sizeof(FxaaConstants),
        sizeof(OcclusionConstants),
        sizeof(ComposeConstants),
        sizeof(InteriorLightConstants),
        sizeof(fsr::RcasConstants),
    };

    for (unsigned slot = 0; slot < kConstantSlotCount; ++slot) {
        D3D11_BUFFER_DESC description = {};
        description.ByteWidth = kByteWidths[slot];
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        const HRESULT result =
            device->CreateBuffer(&description, nullptr, &constants_[slot]);
        if (SUCCEEDED(result)) {
            continue;
        }
        log_message(
            "Falha ao criar constant buffer %u: 0x%08X.", slot,
            static_cast<unsigned>(result));
        return false;
    }
    return true;
}

bool PipelineState::create_sampler_states(ID3D11Device* device) {
    D3D11_SAMPLER_DESC description = {};
    description.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    description.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    description.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    description.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    description.MaxLOD = FLT_MAX;
    HRESULT result =
        device->CreateSamplerState(&description, &linear_sampler_);
    if (FAILED(result)) {
        log_message(
            "Falha ao criar sampler: 0x%08X.",
            static_cast<unsigned>(result));
        return false;
    }

    description.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    result =
        device->CreateSamplerState(&description, &point_sampler_);
    if (FAILED(result)) {
        log_message(
            "Falha ao criar sampler depth: 0x%08X.",
            static_cast<unsigned>(result));
        return false;
    }
    return true;
}

bool PipelineState::create_raster_states(ID3D11Device* device) {
    D3D11_RASTERIZER_DESC rasterizer_description = {};
    rasterizer_description.FillMode = D3D11_FILL_SOLID;
    rasterizer_description.CullMode = D3D11_CULL_NONE;
    rasterizer_description.DepthClipEnable = TRUE;
    HRESULT result = device->CreateRasterizerState(
        &rasterizer_description, &rasterizer_);
    if (FAILED(result)) {
        log_message(
            "Falha ao criar rasterizer state: 0x%08X.",
            static_cast<unsigned>(result));
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depth_description = {};
    depth_description.DepthEnable = FALSE;
    depth_description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth_description.DepthFunc = D3D11_COMPARISON_ALWAYS;
    result = device->CreateDepthStencilState(
        &depth_description, &depth_stencil_);
    if (FAILED(result)) {
        log_message(
            "Falha ao criar depth state: 0x%08X.",
            static_cast<unsigned>(result));
        return false;
    }
    return true;
}

bool PipelineState::create_opaque_blend_state(ID3D11Device* device) {
    D3D11_BLEND_DESC description = {};
    description.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;
    const HRESULT result =
        device->CreateBlendState(&description, &opaque_blend_);
    if (SUCCEEDED(result)) {
        return true;
    }
    log_message(
        "Falha ao criar blend state: 0x%08X.",
        static_cast<unsigned>(result));
    return false;
}

void PipelineState::create_additive_blend_state(ID3D11Device* device) {
    D3D11_BLEND_DESC description = {};
    description.RenderTarget[0].BlendEnable = TRUE;
    description.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    description.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    description.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    description.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    description.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    description.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    description.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;
    const HRESULT result =
        device->CreateBlendState(&description, &additive_blend_);
    if (SUCCEEDED(result)) {
        return;
    }
    log_message(
        "Falha ao criar blend aditivo do bloom: 0x%08X; "
        "o modulo fica indisponivel.",
        static_cast<unsigned>(result));
    safe_release(additive_blend_);
}

}  // namespace photorealism
