#include "pre_tone_effect.hpp"

#include "../postprocess/com_utils.hpp"
#include "../postprocess/device_state.hpp"
#include "../postprocess/shader_compile.hpp"
#include "../runtime.hpp"

#include <cwchar>

namespace photorealism {
namespace passfx {
namespace {

constexpr UINT kPixelSlots = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
constexpr FLOAT kNoBlendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

struct PreToneConstants {
    float exposure_gain;
    float contrast;
    float padding[2];
};

ID3D11PixelShader* compile_pre_tone(ID3D11Device* device) {
    const CompileFromFileFunction compile = resolve_shader_compiler();
    wchar_t path[MAX_PATH] = {};
    std::swprintf(path, MAX_PATH, L"%ls\\shaders\\pre_tone.hlsl", plugin_root());
    ID3DBlob* blob = compile != nullptr
                         ? compile_shader_blob(compile, path, "PSPreTone", "ps_5_0", "PSPreTone")
                         : nullptr;
    ID3D11PixelShader* shader = nullptr;
    if (blob != nullptr) {
        device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader);
    }
    safe_release(blob);
    return shader;
}

bool create_states(
    ID3D11Device* device, ID3D11BlendState** blend, ID3D11DepthStencilState** depth,
    ID3D11RasterizerState** rasterizer) {
    D3D11_BLEND_DESC blend_description = {};
    blend_description.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blend_description.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    blend_description.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_description.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_description.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend_description.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_description.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    D3D11_DEPTH_STENCIL_DESC depth_description = {};
    depth_description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth_description.DepthFunc = D3D11_COMPARISON_ALWAYS;
    D3D11_RASTERIZER_DESC raster_description = {};
    raster_description.FillMode = D3D11_FILL_SOLID;
    raster_description.CullMode = D3D11_CULL_NONE;
    raster_description.DepthClipEnable = TRUE;
    return SUCCEEDED(device->CreateBlendState(&blend_description, blend)) &&
           SUCCEEDED(device->CreateDepthStencilState(&depth_description, depth)) &&
           SUCCEEDED(device->CreateRasterizerState(&raster_description, rasterizer));
}

}

bool PreToneEffect::create(ID3D11Device* device) {
    release();
    device_ = device;
    if (device == nullptr) {
        return false;
    }
    shader_ = compile_pre_tone(device);
    D3D11_BUFFER_DESC buffer = {};
    buffer.ByteWidth = sizeof(PreToneConstants);
    buffer.Usage = D3D11_USAGE_DEFAULT;
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    const bool created = shader_ != nullptr &&
                         SUCCEEDED(device->CreateBuffer(&buffer, nullptr, &constants_)) &&
                         create_states(device, &blend_, &depth_, &rasterizer_);
    log_message(
        created ? "Pre-tom 0.24.1 pronto: shader e estados proprios criados."
                : "Pre-tom 0.24.1 indisponivel: shader ou estados nao foram criados.");
    if (!created) {
        release();
    }
    return created;
}

void PreToneEffect::release() {
    safe_release(scratch_view_);
    safe_release(scratch_);
    safe_release(rasterizer_);
    safe_release(depth_);
    safe_release(blend_);
    safe_release(constants_);
    safe_release(shader_);
    scratch_description_ = {};
}

bool PreToneEffect::ensure_scratch(ID3D11Texture2D* source) {
    D3D11_TEXTURE2D_DESC description = {};
    source->GetDesc(&description);
    const bool same = scratch_ != nullptr && description.Width == scratch_description_.Width &&
                      description.Height == scratch_description_.Height &&
                      description.Format == scratch_description_.Format &&
                      description.MipLevels == scratch_description_.MipLevels &&
                      description.ArraySize == scratch_description_.ArraySize;
    if (same) {
        return true;
    }
    safe_release(scratch_view_);
    safe_release(scratch_);
    scratch_description_ = description;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    description.CPUAccessFlags = 0;
    description.MiscFlags = 0;
    D3D11_SHADER_RESOURCE_VIEW_DESC view = {};
    view.Format = description.Format == DXGI_FORMAT_R16G16B16A16_TYPELESS
                      ? DXGI_FORMAT_R16G16B16A16_FLOAT
                      : description.Format;
    view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = 1;
    const bool created = SUCCEEDED(device_->CreateTexture2D(&description, nullptr, &scratch_)) &&
                         SUCCEEDED(device_->CreateShaderResourceView(scratch_, &view, &scratch_view_));
    if (!created) {
        safe_release(scratch_view_);
        safe_release(scratch_);
        scratch_description_ = {};
    }
    return created;
}

void PreToneEffect::draw(
    ID3D11DeviceContext* context, ID3D11RenderTargetView* hdr,
    ID3D11VertexShader* vertex_shader, const PreToneParameters& parameters) {
    const PreToneConstants constants = {parameters.exposure_gain, parameters.contrast, {0.0f, 0.0f}};
    context->UpdateSubresource(constants_, 0, nullptr, &constants, 0, 0);
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(scratch_description_.Width);
    viewport.Height = static_cast<float>(scratch_description_.Height);
    viewport.MaxDepth = 1.0f;
    ID3D11ShaderResourceView* empty[kPixelSlots] = {};
    context->PSSetShaderResources(0, kPixelSlots, empty);
    context->OMSetRenderTargets(1, &hdr, nullptr);
    context->OMSetBlendState(blend_, kNoBlendFactor, 0xFFFFFFFFu);
    context->OMSetDepthStencilState(depth_, 0);
    context->RSSetState(rasterizer_);
    context->RSSetViewports(1, &viewport);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex_shader, nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->HSSetShader(nullptr, nullptr, 0);
    context->DSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(shader_, nullptr, 0);
    context->PSSetShaderResources(0, 1, &scratch_view_);
    context->PSSetConstantBuffers(0, 1, &constants_);
    context->Draw(3, 0);
    context->PSSetShaderResources(0, kPixelSlots, empty);
}

bool PreToneEffect::apply(
    ID3D11DeviceContext* context, ID3D11RenderTargetView* hdr,
    ID3D11VertexShader* vertex_shader, const PreToneParameters& parameters) {
    if (!ready() || hdr == nullptr || vertex_shader == nullptr) {
        return false;
    }
    ID3D11Resource* resource = nullptr;
    hdr->GetResource(&resource);
    ID3D11Texture2D* texture = nullptr;
    if (resource != nullptr) {
        resource->QueryInterface(IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture));
        resource->Release();
    }
    const bool scratch_ready = texture != nullptr && ensure_scratch(texture);
    if (scratch_ready) {
        SavedState state = {};
        ID3D11ShaderResourceView* game_resources[kPixelSlots] = {};
        capture_state(context, &state);
        context->PSGetShaderResources(0, kPixelSlots, game_resources);
        context->CopyResource(scratch_, texture);
        draw(context, hdr, vertex_shader, parameters);
        restore_state(context, &state);
        context->PSSetShaderResources(0, kPixelSlots, game_resources);
        for (ID3D11ShaderResourceView*& view : game_resources) {
            safe_release(view);
        }
    }
    safe_release(texture);
    return scratch_ready;
}

}
}
