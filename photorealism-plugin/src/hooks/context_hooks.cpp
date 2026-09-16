#include "context_hooks.hpp"

#include "../frame_capture/frame_capture.hpp"
#include "../resource_observer/color_observation.hpp"
#include "../resource_observer/resource_observer.hpp"
#include "../postprocess/postprocess.hpp"
#include "../shader_patch/surface_constants.hpp"
#include "hook_state.hpp"

namespace photorealism {

using namespace hook_state;

void STDMETHODCALLTYPE hooked_set_render_targets(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    ID3D11DepthStencilView* depth_target) {
    const bool from_game = !is_processing_frame();
    if (from_game) {
        observe_depth_target(context, depth_target);
    }
    const OMSetRenderTargetsFunction original =
        g_original_set_render_targets.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context, render_target_count, render_targets, depth_target);
    }
    if (!from_game) {
        return;
    }
    observe_capture_binds(context, render_target_count, render_targets, depth_target);
    shader_patch::bind_surface_constants(
        context, render_target_count, render_targets);
    apply_pass_effects(context, render_target_count, render_targets);
    if (observe_color_targets(
            context, render_target_count, render_targets, depth_target)) {
        reconstruct_game_frame(context, render_targets[0]);
    }
}

void STDMETHODCALLTYPE hooked_set_render_targets_and_uavs(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    ID3D11DepthStencilView* depth_target,
    UINT uav_start_slot,
    UINT uav_count,
    ID3D11UnorderedAccessView* const* unordered_views,
    const UINT* initial_counts) {
    const bool from_game = !is_processing_frame();
    if (from_game) {
        observe_depth_target(context, depth_target);
    }
    const OMSetRenderTargetsAndUavsFunction original =
        g_original_set_render_targets_and_uavs.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(
            context,
            render_target_count,
            render_targets,
            depth_target,
            uav_start_slot,
            uav_count,
            unordered_views,
            initial_counts);
    }
    if (!from_game) {
        return;
    }
    observe_capture_binds(context, render_target_count, render_targets, depth_target);
    shader_patch::bind_surface_constants(
        context, render_target_count, render_targets);
    apply_pass_effects(context, render_target_count, render_targets);
    const bool reconstruct = observe_color_targets(
        context, render_target_count, render_targets, depth_target);
    if (reconstruct && uav_count == 0) {
        reconstruct_game_frame(context, render_targets[0]);
    }
}

void STDMETHODCALLTYPE hooked_clear_depth_stencil_view(
    ID3D11DeviceContext* context,
    ID3D11DepthStencilView* depth_target,
    UINT clear_flags,
    FLOAT depth,
    UINT8 stencil) {
    if (!is_processing_frame()) {
        observe_depth_activity(context, depth_target);
    }
    const ClearDepthStencilViewFunction original =
        g_original_clear_depth_stencil_view.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context, depth_target, clear_flags, depth, stencil);
    }
}

}
