#pragma once

#include "signatures.hpp"

#include <atomic>

namespace photorealism {
namespace hook_state {

extern std::atomic<PresentFunction> g_original_present;
extern std::atomic<Present1Function> g_original_present1;
extern std::atomic<bool> g_present1_required;
extern std::atomic<ResizeBuffersFunction> g_original_resize_buffers;
extern std::atomic<OMSetRenderTargetsFunction> g_original_set_render_targets;
extern std::atomic<OMSetRenderTargetsAndUavsFunction>
    g_original_set_render_targets_and_uavs;
extern std::atomic<ClearDepthStencilViewFunction>
    g_original_clear_depth_stencil_view;
extern std::atomic<void**> g_present_vtable_entry;
extern std::atomic<void**> g_present1_vtable_entry;
extern std::atomic<bool> g_present_runtime_audited;
extern std::atomic<bool> g_present1_runtime_audited;
extern thread_local unsigned g_present_dispatch_depth;

class PresentDispatchScope {
  public:
    PresentDispatchScope() : process_(g_present_dispatch_depth++ == 0) {}
    ~PresentDispatchScope() { --g_present_dispatch_depth; }

    bool should_process() const { return process_; }

  private:
    bool process_;
};
}
}
