#include "bloom_pyramid.hpp"

#include "../runtime.hpp"
#include "com_utils.hpp"
#include "format_utils.hpp"

#include <algorithm>

namespace photorealism {






void BloomPyramid::render(
    const D3D11_TEXTURE2D_DESC& description, const BloomFrame& frame) {
    BloomConstants constants = {};
    constants.threshold = frame.threshold;
    constants.knee = frame.knee;
    constants.input_needs_srgb_decode =
        frame.scene_needs_srgb_decode ? 1.0f : 0.0f;
    constants.filter_radius[0] = 1.0f;
    constants.filter_radius[1] = 1.0f;

    D3D11_VIEWPORT viewport = {};
    viewport.MaxDepth = 1.0f;

    for (UINT index = 0; index < level_count_; ++index) {
        const UINT source_width =
            index == 0 ? description.Width : widths_[index - 1];
        const UINT source_height =
            index == 0 ? description.Height : heights_[index - 1];
        constants.source_texel_size[0] =
            1.0f / static_cast<float>(source_width);
        constants.source_texel_size[1] =
            1.0f / static_cast<float>(source_height);
        context_->UpdateSubresource(
            constant_buffer_, 0, nullptr, &constants, 0, 0);

        viewport.Width = static_cast<float>(widths_[index]);
        viewport.Height = static_cast<float>(heights_[index]);
        context_->RSSetViewports(1, &viewport);
        context_->OMSetRenderTargets(1, &targets_[index], nullptr);
        context_->PSSetShader(
            index == 0 ? frame.shaders->bloom_bright() : frame.shaders->bloom_downsample(),
            nullptr,
            0);
        ID3D11ShaderResourceView* source_view =
            index == 0 ? frame.scene_view : views_[index - 1];
        context_->PSSetShaderResources(0, 1, &source_view);
        context_->PSSetSamplers(0, 1, &frame.sampler);
        context_->PSSetConstantBuffers(0, 1, &constant_buffer_);
        context_->Draw(3, 0);
        context_->OMSetRenderTargets(0, nullptr, nullptr);
    }

    context_->OMSetBlendState(
        frame.additive_blend, nullptr, 0xFFFFFFFFu);
    for (UINT index = level_count_ - 1; index > 0; --index) {
        constants.source_texel_size[0] =
            1.0f / static_cast<float>(widths_[index]);
        constants.source_texel_size[1] =
            1.0f / static_cast<float>(heights_[index]);
        context_->UpdateSubresource(
            constant_buffer_, 0, nullptr, &constants, 0, 0);

        viewport.Width = static_cast<float>(widths_[index - 1]);
        viewport.Height = static_cast<float>(heights_[index - 1]);
        context_->RSSetViewports(1, &viewport);
        context_->OMSetRenderTargets(
            1, &targets_[index - 1], nullptr);
        context_->PSSetShader(frame.shaders->bloom_upsample(), nullptr, 0);
        context_->PSSetShaderResources(0, 1, &views_[index]);
        context_->PSSetSamplers(0, 1, &frame.sampler);
        context_->PSSetConstantBuffers(0, 1, &constant_buffer_);
        context_->Draw(3, 0);
        context_->OMSetRenderTargets(0, nullptr, nullptr);
        ID3D11ShaderResourceView* null_view = nullptr;
        context_->PSSetShaderResources(0, 1, &null_view);
    }
    context_->OMSetBlendState(frame.opaque_blend, nullptr, 0xFFFFFFFFu);

    viewport.Width = static_cast<float>(description.Width);
    viewport.Height = static_cast<float>(description.Height);
    context_->RSSetViewports(1, &viewport);
}

void BloomPyramid::draw_preview(
    ID3D11RenderTargetView* output,
    const BloomFrame& frame,
    bool output_needs_srgb_encode) {
    context_->OMSetRenderTargets(1, &output, nullptr);
    context_->PSSetShader(frame.shaders->bloom_upsample(), nullptr, 0);
    BloomConstants preview_constants = {};
    preview_constants.source_texel_size[0] =
        1.0f / static_cast<float>(widths_[0]);
    preview_constants.source_texel_size[1] =
        1.0f / static_cast<float>(heights_[0]);
    preview_constants.threshold = frame.threshold;
    preview_constants.knee = frame.knee;
    preview_constants.output_needs_srgb_encode =
        output_needs_srgb_encode ? 1.0f : 0.0f;
    context_->UpdateSubresource(
        constant_buffer_, 0, nullptr, &preview_constants, 0, 0);
    context_->PSSetShaderResources(0, 1, &views_[0]);
    context_->PSSetSamplers(0, 1, &frame.sampler);
    context_->PSSetConstantBuffers(0, 1, &constant_buffer_);
    context_->Draw(3, 0);
}

}  // namespace photorealism
