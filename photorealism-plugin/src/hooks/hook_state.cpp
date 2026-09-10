#include "hook_state.hpp"

namespace photorealism {
namespace hook_state {

std::atomic<PresentFunction> g_original_present{nullptr};
std::atomic<Present1Function> g_original_present1{nullptr};
std::atomic<bool> g_present1_required{false};
std::atomic<ResizeBuffersFunction> g_original_resize_buffers{nullptr};
std::atomic<OMSetRenderTargetsFunction> g_original_set_render_targets{nullptr};
std::atomic<OMSetRenderTargetsAndUavsFunction>
    g_original_set_render_targets_and_uavs{nullptr};
std::atomic<ClearDepthStencilViewFunction>
    g_original_clear_depth_stencil_view{nullptr};
std::atomic<void**> g_present_vtable_entry{nullptr};
std::atomic<void**> g_present1_vtable_entry{nullptr};
std::atomic<bool> g_present_runtime_audited{false};
std::atomic<bool> g_present1_runtime_audited{false};
thread_local unsigned g_present_dispatch_depth = 0;
}
}
