#include "fsr_states.hpp"

#include "../postprocess/com_utils.hpp"
#include "../runtime.hpp"

namespace photorealism {
namespace fsr {
namespace {

constexpr FLOAT kNoBlendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

bool create_blend(ID3D11Device* device, ID3D11BlendState** state) {
    D3D11_BLEND_DESC description = {};
    description.RenderTarget[0].BlendEnable = FALSE;
    description.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    description.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    description.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    description.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    description.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    description.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    description.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;
    return SUCCEEDED(device->CreateBlendState(&description, state));
}

bool create_depth(ID3D11Device* device, ID3D11DepthStencilState** state) {
    D3D11_DEPTH_STENCIL_DESC description = {};
    description.DepthEnable = FALSE;
    description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    description.DepthFunc = D3D11_COMPARISON_ALWAYS;
    description.StencilEnable = FALSE;
    return SUCCEEDED(device->CreateDepthStencilState(&description, state));
}

bool create_rasterizer(ID3D11Device* device, ID3D11RasterizerState** state) {
    D3D11_RASTERIZER_DESC description = {};
    description.FillMode = D3D11_FILL_SOLID;
    description.CullMode = D3D11_CULL_NONE;
    description.DepthClipEnable = TRUE;
    description.ScissorEnable = FALSE;
    return SUCCEEDED(device->CreateRasterizerState(&description, state));
}

}

bool FsrStates::create(ID3D11Device* device) {
    release();
    if (device == nullptr) {
        return false;
    }
    const bool created = create_blend(device, &blend_) &&
                         create_depth(device, &depth_) &&
                         create_rasterizer(device, &rasterizer_);
    if (!created) {
        log_message(
            "FSR nao criou os estados proprios do desenho; sem eles o RCAS "
            "herdaria o blend e o scissor da interface do jogo.");
        release();
    }
    return created;
}

void FsrStates::release() {
    safe_release(rasterizer_);
    safe_release(depth_);
    safe_release(blend_);
}

void FsrStates::bind(ID3D11DeviceContext* context) const {
    context->OMSetBlendState(blend_, kNoBlendFactor, 0xFFFFFFFFu);
    context->OMSetDepthStencilState(depth_, 0);
    context->RSSetState(rasterizer_);
}

}
}
