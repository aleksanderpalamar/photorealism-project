#include "frame_passes.hpp"

namespace photorealism {
namespace {

using PassDraw =
    void (*)(const FramePassScene&, const FramePassPlan&, const PassIo&);

constexpr PassDraw kPassDraws[kEffectPassCount] = {
    draw_fxaa_pass,
    draw_visual_pass,
    draw_interior_light_pass,
    draw_occlusion_pass,
    draw_temporal_pass,
    draw_sharpen_pass,
};

const IntermediateTarget* intermediate_for(
    const FramePassScene& scene, ChainSlot slot) {
    if (slot == ChainSlot::First) {
        return &scene.resources->first();
    }
    return slot == ChainSlot::Second ? &scene.resources->second() : nullptr;
}

PassIo io_for(const FramePassScene& scene, const EffectStep& step) {
    const IntermediateTarget* source = intermediate_for(scene, step.source);
    const IntermediateTarget* target = intermediate_for(scene, step.target);
    PassIo io = {};
    io.source = source != nullptr ? source->view : scene.resources->scene_view();
    io.raw_source = source != nullptr ? source->raw_view : nullptr;
    io.target = target != nullptr ? target->target : scene.output;
    io.target_texture = target != nullptr ? target->texture : scene.back_buffer;
    return io;
}

}

void run_effect_chain(const FramePassScene& scene, const FramePassPlan& plan) {
    for (unsigned index = 0; index < scene.chain->count; ++index) {
        const EffectStep& step = scene.chain->steps[index];
        kPassDraws[static_cast<unsigned>(step.pass)](scene, plan, io_for(scene, step));
    }
}

}
