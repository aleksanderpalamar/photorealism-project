#include "context_hooks.hpp"

#include "../resource_observer.hpp"
#include "../postprocess/postprocess.hpp"
#include "hook_state.hpp"

namespace photorealism {

using namespace hook_state;

void STDMETHODCALLTYPE hooked_set_render_targets(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    ID3D11DepthStencilView* depth_target) {
    if (!is_processing_frame()) {
        observe_depth_target(context, depth_target);
    }
    const OMSetRenderTargetsFunction original =
        g_original_set_render_targets.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context, render_target_count, render_targets, depth_target);
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
    if (!is_processing_frame()) {
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
