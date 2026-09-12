#include "upscale_pipeline.hpp"

#include "fsr_telemetry.hpp"

namespace photorealism {
namespace fsr {
namespace {

ID3D11ShaderResourceView* const kNoResource[1] = {nullptr};
ID3D11UnorderedAccessView* const kNoAccess[1] = {nullptr};

}

bool UpscalePipeline::ensure(
    ID3D11Device* device, unsigned width, unsigned height) {
    if (device == nullptr) {
        return false;
    }
    if (device_ != device) {
        release();
        if (!shaders_.create(device)) {
            return false;
        }
        device_ = device;
    }
    return resources_.ensure(device, width, height);
}

void UpscalePipeline::release() {
    resources_.release();
    shaders_.release();
    device_ = nullptr;
}

void UpscalePipeline::dispatch_easu(
    ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* source,
    const RenderExtent& internal,
    unsigned width,
    unsigned height) {
    EasuConstants constants = {};
    constants.input_size[0] = static_cast<float>(internal.width);
    constants.input_size[1] = static_cast<float>(internal.height);
    constants.output_size[0] = static_cast<float>(width);
    constants.output_size[1] = static_cast<float>(height);
    constants.input_texel_size[0] = 1.0f / static_cast<float>(internal.width);
    constants.input_texel_size[1] = 1.0f / static_cast<float>(internal.height);
    context->UpdateSubresource(
        shaders_.easu_constants(), 0, nullptr, &constants, 0, 0);

    ID3D11UnorderedAccessView* access = resources_.access();
    ID3D11Buffer* buffer = shaders_.easu_constants();
    context->CSSetShader(shaders_.easu(), nullptr, 0);
    context->CSSetShaderResources(0, 1, &source);
    context->CSSetUnorderedAccessViews(0, 1, &access, nullptr);
    context->CSSetConstantBuffers(0, 1, &buffer);
    context->Dispatch(dispatch_groups(width), dispatch_groups(height), 1);
    telemetry().record_dispatch();

    context->CSSetUnorderedAccessViews(
        0, 1, const_cast<ID3D11UnorderedAccessView* const*>(kNoAccess),
        nullptr);
    context->CSSetShaderResources(
        0, 1, const_cast<ID3D11ShaderResourceView* const*>(kNoResource));
    context->CSSetShader(nullptr, nullptr, 0);
}

void UpscalePipeline::draw_rcas(
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* output,
    unsigned width,
    unsigned height,
    float sharpness) {
    RcasConstants constants = {};
    constants.output_size[0] = static_cast<float>(width);
    constants.output_size[1] = static_cast<float>(height);
    constants.attenuation = sharpness;
    context->UpdateSubresource(
        shaders_.rcas_constants(), 0, nullptr, &constants, 0, 0);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MaxDepth = 1.0f;

    ID3D11ShaderResourceView* upscaled = resources_.view();
    ID3D11Buffer* buffer = shaders_.rcas_constants();
    context->OMSetRenderTargets(1, &output, nullptr);
    context->RSSetViewports(1, &viewport);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(shaders_.rcas_vertex(), nullptr, 0);
    context->PSSetShader(shaders_.rcas_pixel(), nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->HSSetShader(nullptr, nullptr, 0);
    context->DSSetShader(nullptr, nullptr, 0);
    context->PSSetShaderResources(0, 1, &upscaled);
    context->PSSetConstantBuffers(0, 1, &buffer);
    context->Draw(3, 0);

    context->PSSetShaderResources(
        0, 1, const_cast<ID3D11ShaderResourceView* const*>(kNoResource));
}

bool UpscalePipeline::run(
    ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* source,
    const RenderExtent& internal,
    ID3D11RenderTargetView* output,
    unsigned width,
    unsigned height,
    float sharpness) {
    if (context == nullptr || source == nullptr || output == nullptr) {
        return false;
    }
    if (!ready() || internal.width == 0 || internal.height == 0) {
        return false;
    }
    dispatch_easu(context, source, internal, width, height);
    draw_rcas(context, output, width, height, sharpness);
    return true;
}

}
}
