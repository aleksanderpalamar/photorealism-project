#include "frame_passes.hpp"

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

void draw_depth_preview(const FramePassScene& scene) {
    scene.context->OMSetRenderTargets(1, &scene.output, nullptr);
    scene.context->PSSetShader(scene.shaders->depth_preview(), nullptr, 0);
    ID3D11ShaderResourceView* depth_view = scene.depth->view();
    scene.context->PSSetShaderResources(0, 1, &depth_view);
    scene.context->PSSetSamplers(0, 1, scene.pipeline->point_sampler_address());
    scene.context->PSSetConstantBuffers(
        0, 1, scene.pipeline->constants_address(ConstantSlot::DepthPreview));
    scene.context->Draw(3, 0);
}

void draw_occlusion_preview(const FramePassScene& scene, const FramePassPlan& plan) {
    const PassIo io = {scene.resources->scene_view(), nullptr, scene.output,
                       scene.back_buffer};
    draw_occlusion_pass(scene, plan, io);
}

}

void compose_output(const FramePassScene& scene, const FramePassPlan& plan) {
    if (plan.bloom_preview) {
        scene.bloom->draw_preview(
            scene.output, bloom_frame(scene), scene.output_needs_srgb_encode);
        return;
    }
    if (plan.depth_preview) {
        draw_depth_preview(scene);
        return;
    }
    if (plan.ssao_preview) {
        draw_occlusion_preview(scene, plan);
        return;
    }
    run_effect_chain(scene, plan);
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
    scene.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    scene.context->VSSetShader(scene.shaders->vertex(), nullptr, 0);
    scene.context->GSSetShader(nullptr, nullptr, 0);
    scene.context->HSSetShader(nullptr, nullptr, 0);
    scene.context->DSSetShader(nullptr, nullptr, 0);
}

}
