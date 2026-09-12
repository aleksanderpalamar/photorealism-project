#include "renderer.hpp"

#include "../postprocess/com_utils.hpp"

#include <cstring>

namespace photorealism {
namespace overlay {

bool Renderer::create(ID3D11Device* device, const BakedFont& font) {
    release();
    if (device == nullptr) {
        return false;
    }
    device_ = device;
    const bool built = create_shaders(device) &&
                       create_states(device, font.smooth) &&
                       create_constants(device) &&
                       create_font_texture(device, font) &&
                       ensure_capacity(kInitialVertexCapacity);
    if (!built) {
        release();
        return false;
    }
    return true;
}

void Renderer::release() {
    safe_release(font_view_);
    safe_release(font_texture_);
    safe_release(sampler_);
    safe_release(rasterizer_);
    safe_release(depth_);
    safe_release(blend_);
    safe_release(constants_);
    safe_release(vertex_view_);
    safe_release(vertices_);
    safe_release(pixel_shader_);
    safe_release(vertex_shader_);
    capacity_ = 0;
    device_ = nullptr;
}

bool Renderer::upload(ID3D11DeviceContext* context, const DrawList& list) {
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT result =
        context->Map(vertices_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(result) || mapped.pData == nullptr) {
        return false;
    }
    std::memcpy(
        mapped.pData,
        list.vertices().data(),
        list.vertex_count() * sizeof(Vertex));
    context->Unmap(vertices_, 0);
    return true;
}

void Renderer::bind(
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* target,
    const DrawList& list) {
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = list.width();
    viewport.Height = list.height();
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    constexpr FLOAT kNoBlendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context->OMSetRenderTargets(1, &target, nullptr);
    context->RSSetViewports(1, &viewport);
    context->OMSetBlendState(blend_, kNoBlendFactor, 0xFFFFFFFF);
    context->OMSetDepthStencilState(depth_, 0);
    context->RSSetState(rasterizer_);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex_shader_, nullptr, 0);
    context->PSSetShader(pixel_shader_, nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->HSSetShader(nullptr, nullptr, 0);
    context->DSSetShader(nullptr, nullptr, 0);
    context->VSSetShaderResources(0, 1, &vertex_view_);
    context->PSSetShaderResources(0, 1, &font_view_);
    context->PSSetSamplers(0, 1, &sampler_);
    context->PSSetConstantBuffers(0, 1, &constants_);
}

void Renderer::draw(
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* target,
    const DrawList& list,
    bool output_needs_srgb_encode) {
    if (!ready() || context == nullptr || target == nullptr) {
        return;
    }
    if (list.vertex_count() == 0) {
        return;
    }
    if (!ensure_capacity(list.vertex_count())) {
        return;
    }
    if (!upload(context, list)) {
        return;
    }

    OverlayConstants constants = {};
    constants.output_needs_srgb_encode = output_needs_srgb_encode ? 1.0f : 0.0f;
    context->UpdateSubresource(constants_, 0, nullptr, &constants, 0, 0);

    bind(context, target, list);
    context->Draw(static_cast<UINT>(list.vertex_count()), 0);
}

}
}
