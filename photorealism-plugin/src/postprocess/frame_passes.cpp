#include "frame_passes.hpp"

#include "../runtime.hpp"

namespace photorealism {
namespace {

BloomFrame bloom_frame(const FramePassScene& scene) {
    BloomFrame frame = {};
    frame.shaders = scene.shaders;
    frame.sampler = scene.pipeline->linear_sampler();
    frame.scene_view = scene.resources->scene_view();
    frame.additive_blend = scene.pipeline->additive_blend();
    frame.opaque_blend = scene.pipeline->opaque_blend();
    frame.scene_needs_srgb_decode = scene.resources->scene_needs_srgb_decode();
    return frame;
}

void draw_visual_pass(
    const FramePassScene& scene,
    const FramePassPlan& plan,
    ID3D11RenderTargetView* target) {
    scene.context->OMSetRenderTargets(1, &target, nullptr);
    scene.context->PSSetShader(scene.shaders->visual(), nullptr, 0);
    ID3D11ShaderResourceView* visual_resources[2] = {
        scene.resources->scene_view(), plan.bloom ? scene.bloom->top_view() : nullptr};
    scene.context->PSSetShaderResources(0, 2, visual_resources);
    scene.context->PSSetSamplers(0, 1, scene.pipeline->linear_sampler_address());
    scene.context->PSSetConstantBuffers(0, 1, scene.pipeline->visual_constants_address());
    scene.context->Draw(3, 0);
}

void draw_ssao_pass(
    const FramePassScene& scene,
    ID3D11RenderTargetView* target,
    ID3D11ShaderResourceView* source) {
    scene.context->OMSetRenderTargets(1, &target, nullptr);
    scene.context->PSSetShader(scene.shaders->ssao(), nullptr, 0);
    ID3D11ShaderResourceView* ssao_resources[2] = {
        source, scene.depth->view()};
    ID3D11SamplerState* ssao_samplers[2] = {
        scene.pipeline->linear_sampler(), scene.pipeline->point_sampler()};
    scene.context->PSSetShaderResources(0, 2, ssao_resources);
    scene.context->PSSetSamplers(0, 2, ssao_samplers);
    scene.context->PSSetConstantBuffers(0, 1, scene.pipeline->ssao_constants_address());
    scene.context->Draw(3, 0);
}

void draw_depth_preview(const FramePassScene& scene) {
    scene.context->OMSetRenderTargets(1, &scene.output, nullptr);
    scene.context->PSSetShader(scene.shaders->depth_preview(), nullptr, 0);
    ID3D11ShaderResourceView* depth_view = scene.depth->view();
    scene.context->PSSetShaderResources(0, 1, &depth_view);
    scene.context->PSSetSamplers(0, 1, scene.pipeline->point_sampler_address());
    scene.context->PSSetConstantBuffers(0, 1, scene.pipeline->depth_constants_address());
    scene.context->Draw(3, 0);
}

void draw_temporal_chain(
    const FramePassScene& scene, const FramePassPlan& plan) {
    draw_visual_pass(scene, plan, scene.resources->visual_target());

    scene.context->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* current_view = scene.resources->visual_view();
    if (plan.ssao) {
        draw_ssao_pass(scene, scene.resources->spatial_target(), scene.resources->visual_view());
        scene.context->OMSetRenderTargets(0, nullptr, nullptr);
        ID3D11ShaderResourceView* null_ssao_resources[2] = {};
        scene.context->PSSetShaderResources(0, 2, null_ssao_resources);
        current_view = scene.resources->spatial_view();
    }

    scene.context->OMSetRenderTargets(1, &scene.output, nullptr);
    scene.context->PSSetShader(scene.shaders->temporal(), nullptr, 0);
    ID3D11ShaderResourceView* temporal_resources[4] = {
        current_view,
        scene.temporal->valid() ? scene.temporal->color_view() : nullptr,
        scene.depth->view(),
        scene.temporal->valid() ? scene.temporal->depth_view() : nullptr};
    ID3D11SamplerState* temporal_samplers[2] = {
        scene.pipeline->linear_sampler(), scene.pipeline->point_sampler()};
    scene.context->PSSetShaderResources(0, 4, temporal_resources);
    scene.context->PSSetSamplers(0, 2, temporal_samplers);
    scene.context->PSSetConstantBuffers(0, 1, scene.pipeline->temporal_constants_address());
    scene.context->Draw(3, 0);

    scene.context->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* null_temporal_resources[4] = {};
    scene.context->PSSetShaderResources(0, 4, null_temporal_resources);
    scene.temporal->store(scene.back_buffer, scene.depth->texture());
    if (scene.temporal->valid()) {
        return;
    }
    scene.temporal->mark_valid();
    log_message(
        "Historico temporal 0.10.0 inicializado: color=%ux%u "
        "depth=%ux%u generation=%llu.",
        scene.description.Width,
        scene.description.Height,
        scene.depth_description.Width,
        scene.depth_description.Height,
        static_cast<unsigned long long>(scene.depth_generation));
}

void draw_ssao_chain(
    const FramePassScene& scene, const FramePassPlan& plan) {
    draw_visual_pass(scene, plan, scene.resources->visual_target());
    scene.context->OMSetRenderTargets(0, nullptr, nullptr);
    draw_ssao_pass(scene, scene.output, scene.resources->visual_view());
}

}  // namespace


void compose_output(const FramePassScene& scene, const FramePassPlan& plan) {
    if (plan.bloom_preview) {
        scene.bloom->draw_preview(
            scene.output,
            bloom_frame(scene),
            scene.output_needs_srgb_encode);
        return;
    }
    if (plan.depth_preview) {
        draw_depth_preview(scene);
        return;
    }
    if (plan.ssao_preview) {
        draw_ssao_pass(scene, scene.output, scene.resources->scene_view());
        return;
    }
    if (plan.temporal) {
        draw_temporal_chain(scene, plan);
        return;
    }
    if (plan.ssao) {
        draw_ssao_chain(scene, plan);
        return;
    }
    draw_visual_pass(scene, plan, scene.output);
}

void bind_common_pipeline_state(const FramePassScene& scene) {
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(scene.description.Width);
    viewport.Height = static_cast<float>(scene.description.Height);
    viewport.MaxDepth = 1.0f;

    scene.context->OMSetBlendState(scene.pipeline->opaque_blend(), nullptr, 0xFFFFFFFFu);
    scene.context->OMSetDepthStencilState(scene.pipeline->depth_stencil(), 0);
    scene.context->RSSetState(scene.pipeline->rasterizer());
    scene.context->RSSetViewports(1, &viewport);
    scene.context->IASetInputLayout(nullptr);
    scene.context->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    scene.context->VSSetShader(scene.shaders->vertex(), nullptr, 0);
    scene.context->GSSetShader(nullptr, nullptr, 0);
    scene.context->HSSetShader(nullptr, nullptr, 0);
    scene.context->DSSetShader(nullptr, nullptr, 0);
}

}  // namespace photorealism
