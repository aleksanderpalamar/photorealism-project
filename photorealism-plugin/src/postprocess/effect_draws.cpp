#include "frame_passes.hpp"

#include "../runtime.hpp"

namespace photorealism {
namespace {

constexpr UINT kMaximumPassResources = 4;

void set_viewport(const FramePassScene& scene, UINT width, UINT height) {
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MaxDepth = 1.0f;
    scene.context->RSSetViewports(1, &viewport);
}

void unbind(const FramePassScene& scene) {
    scene.context->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* empty[kMaximumPassResources] = {};
    scene.context->PSSetShaderResources(0, kMaximumPassResources, empty);
}

void draw_fullscreen(
    const FramePassScene& scene, ID3D11RenderTargetView* target,
    ID3D11PixelShader* shader, ConstantSlot slot, UINT resource_count,
    ID3D11ShaderResourceView* const* resources) {
    scene.context->OMSetRenderTargets(1, &target, nullptr);
    scene.context->PSSetShader(shader, nullptr, 0);
    scene.context->PSSetShaderResources(0, resource_count, resources);
    ID3D11SamplerState* samplers[2] = {
        scene.pipeline->linear_sampler(), scene.pipeline->point_sampler()};
    scene.context->PSSetSamplers(0, 2, samplers);
    scene.context->PSSetConstantBuffers(0, 1, scene.pipeline->constants_address(slot));
    scene.context->Draw(3, 0);
    unbind(scene);
}

void log_history_start(const FramePassScene& scene) {
    log_message(
        "Historico temporal 0.10.0 inicializado: color=%ux%u "
        "depth=%ux%u generation=%llu.",
        scene.description.Width, scene.description.Height,
        scene.depth_description.Width, scene.depth_description.Height,
        static_cast<unsigned long long>(scene.depth_generation));
}

}

void draw_fxaa_pass(const FramePassScene& scene, const FramePassPlan&, const PassIo& io) {
    ID3D11ShaderResourceView* resources[1] = {io.source};
    draw_fullscreen(
        scene, io.target, scene.effects->shader(EffectShader::Fxaa),
        ConstantSlot::Fxaa, 1, resources);
}

void draw_visual_pass(
    const FramePassScene& scene, const FramePassPlan& plan, const PassIo& io) {
    ID3D11ShaderResourceView* resources[2] = {
        io.source, plan.bloom ? scene.bloom->top_view() : nullptr};
    draw_fullscreen(
        scene, io.target, scene.shaders->visual(), ConstantSlot::Visual, 2,
        resources);
}

void draw_interior_light_pass(
    const FramePassScene& scene, const FramePassPlan&, const PassIo& io) {
    ID3D11ShaderResourceView* resources[2] = {io.source, scene.depth->view()};
    draw_fullscreen(
        scene, io.target, scene.effects->shader(EffectShader::InteriorLight),
        ConstantSlot::InteriorLight, 2, resources);
}

void draw_occlusion_pass(
    const FramePassScene& scene, const FramePassPlan&, const PassIo& io) {
    set_viewport(scene, scene.occlusion->width(), scene.occlusion->height());
    ID3D11ShaderResourceView* depth_resources[1] = {scene.depth->view()};
    draw_fullscreen(
        scene, scene.occlusion->target(),
        scene.effects->shader(EffectShader::Occlusion), ConstantSlot::Occlusion,
        1, depth_resources);
    set_viewport(scene, scene.description.Width, scene.description.Height);

    ID3D11ShaderResourceView* resources[3] = {
        io.source, scene.occlusion->view(), scene.depth->view()};
    draw_fullscreen(
        scene, io.target, scene.effects->shader(EffectShader::Compose),
        ConstantSlot::Compose, 3, resources);
}

void draw_temporal_pass(
    const FramePassScene& scene, const FramePassPlan&, const PassIo& io) {
    const bool history = scene.temporal->valid();
    ID3D11ShaderResourceView* resources[4] = {
        io.source, history ? scene.temporal->color_view() : nullptr,
        scene.depth->view(), history ? scene.temporal->depth_view() : nullptr};
    draw_fullscreen(
        scene, io.target, scene.shaders->temporal(), ConstantSlot::Temporal, 4,
        resources);
    scene.temporal->store(io.target_texture, scene.depth->texture());
    if (history) {
        return;
    }
    scene.temporal->mark_valid();
    log_history_start(scene);
}

void draw_sharpen_pass(
    const FramePassScene& scene, const FramePassPlan&, const PassIo& io) {
    ID3D11ShaderResourceView* resources[2] = {io.raw_source, nullptr};
    draw_fullscreen(
        scene, io.target, scene.effects->shader(EffectShader::Sharpen),
        ConstantSlot::Sharpen, 2, resources);
}

}
