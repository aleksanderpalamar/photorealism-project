#include "color_observation.hpp"

#include "../runtime.hpp"
#include "color_capture.hpp"
#include "frame_transition.hpp"
#include "pass_trace.hpp"
#include "view_shape.hpp"

#include <atomic>
#include <vector>
#include <windows.h>

namespace photorealism {
namespace {

SRWLOCK g_color_lock = SRWLOCK_INIT;
std::atomic<bool> g_color_capture_active{false};
observer::FrameTransition g_transition;
observer::ColorCapture g_capture;
observer::PassTrace g_trace;
observer::TraceArming g_arming;
bool g_captured_this_frame = false;
UINT g_configured[4] = {};

void record_trace(
    ID3D11Texture2D* texture,
    const observer::TargetShape& shape,
    const observer::TransitionStep& step,
    UINT render_target_count,
    ID3D11DepthStencilView* depth_target) {
    observer::TraceEntry entry;
    entry.texture = texture;
    entry.shape = shape;
    entry.targets = render_target_count;
    entry.role = step.role;
    entry.captured = step.capture;
    entry.reconstruct = step.reconstruct;
    ID3D11Texture2D* depth = nullptr;
    observer::TargetShape depth_shape;
    if (observer::describe_view(depth_target, &depth, &depth_shape)) {
        entry.depth_width = depth_shape.width;
        entry.depth_height = depth_shape.height;
        depth->Release();
    }
    g_trace.record(entry);
}

}

void enable_color_capture(
    ID3D11Texture2D* output,
    UINT output_width,
    UINT output_height,
    UINT expected_width,
    UINT expected_height) {
    const UINT wanted[4] = {
        output_width, output_height, expected_width, expected_height};
    AcquireSRWLockExclusive(&g_color_lock);
    const bool changed = g_configured[0] != wanted[0] ||
                         g_configured[1] != wanted[1] ||
                         g_configured[2] != wanted[2] ||
                         g_configured[3] != wanted[3];
    for (UINT index = 0; index < 4; ++index) {
        g_configured[index] = wanted[index];
    }
    g_transition.configure(output, expected_width, expected_height);
    ReleaseSRWLockExclusive(&g_color_lock);
    g_color_capture_active.store(true, std::memory_order_release);
    if (changed) {
        log_message(
            "FSR procura o quadro interno perto de %ux%u para a saida %ux%u.",
            expected_width,
            expected_height,
            output_width,
            output_height);
    }
}

bool observe_color_targets(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    ID3D11DepthStencilView* depth_target) {
    if (!g_color_capture_active.load(std::memory_order_acquire)) {
        return false;
    }
    if (render_targets == nullptr || render_target_count == 0) {
        return false;
    }
    ID3D11Texture2D* texture = nullptr;
    observer::TargetShape shape;
    if (!observer::describe_view(render_targets[0], &texture, &shape)) {
        return false;
    }

    AcquireSRWLockExclusive(&g_color_lock);
    const observer::TransitionStep step = g_transition.observe(texture, shape);
    if (step.acquired != nullptr) {
        texture->AddRef();
    }
    observer::release_texture_handle(step.released);
    if (step.capture != nullptr) {
        g_captured_this_frame =
            g_capture.copy_from(
                context, static_cast<ID3D11Texture2D*>(step.capture)) ||
            g_captured_this_frame;
    }
    if (g_trace.recording()) {
        record_trace(texture, shape, step, render_target_count, depth_target);
    }
    observer::release_texture_handle(step.capture);
    const bool reconstruct = step.reconstruct && g_captured_this_frame;
    ReleaseSRWLockExclusive(&g_color_lock);
    texture->Release();
    return reconstruct;
}

bool acquire_captured_frame(
    ID3D11ShaderResourceView** view, UINT* width, UINT* height) {
    if (view == nullptr || width == nullptr || height == nullptr) {
        return false;
    }
    AcquireSRWLockExclusive(&g_color_lock);
    const bool available =
        g_captured_this_frame && g_capture.view() != nullptr;
    if (available) {
        *view = g_capture.view();
        (*view)->AddRef();
        *width = g_capture.width();
        *height = g_capture.height();
    }
    ReleaseSRWLockExclusive(&g_color_lock);
    return available;
}

bool color_frame_captured() {
    AcquireSRWLockExclusive(&g_color_lock);
    const bool captured = g_captured_this_frame;
    ReleaseSRWLockExclusive(&g_color_lock);
    return captured;
}

void end_color_frame() {
    if (!g_color_capture_active.load(std::memory_order_acquire)) {
        return;
    }
    std::vector<observer::TraceEntry> entries;
    bool truncated = false;

    AcquireSRWLockExclusive(&g_color_lock);
    if (g_arming.end_frame(g_transition.binds())) {
        g_trace.arm();
    }
    observer::release_texture_handle(g_transition.end_frame());
    g_captured_this_frame = false;
    g_trace.end_frame();
    const bool flush = g_trace.take(&entries, &truncated);
    ReleaseSRWLockExclusive(&g_color_lock);

    if (flush) {
        observer::log_trace(entries, truncated);
    }
}

void disable_color_capture() {
    g_color_capture_active.store(false, std::memory_order_release);
    reset_color_discovery();
}

void reset_color_discovery() {
    AcquireSRWLockExclusive(&g_color_lock);
    observer::release_texture_handle(g_transition.end_frame());
    g_transition.configure(nullptr, 0, 0);
    g_arming = observer::TraceArming();
    g_capture.release();
    g_captured_this_frame = false;
    for (UINT& value : g_configured) {
        value = 0;
    }
    ReleaseSRWLockExclusive(&g_color_lock);
}

}
