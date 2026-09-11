#include "swap_chain_hooks.hpp"

#include "../postprocess/postprocess.hpp"
#include "../runtime.hpp"
#include "../steam/steam_screenshots.hpp"
#include "hook_audit.hpp"
#include "hook_state.hpp"

namespace photorealism {

using namespace hook_state;

HRESULT STDMETHODCALLTYPE hooked_present(
    IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {
    PresentDispatchScope dispatch;
    if (dispatch.should_process()) {
        process_frame(swap_chain);
        observe_postprocessed_frame(swap_chain);
    }
    if (!g_present_runtime_audited.exchange(true, std::memory_order_acq_rel)) {
        log_present_entry(
            "first-runtime-call",
            "Present",
            g_present_vtable_entry.load(std::memory_order_acquire),
            reinterpret_cast<void*>(&hooked_present),
            reinterpret_cast<void*>(
                g_original_present.load(std::memory_order_acquire)));
    }
    const PresentFunction original =
        g_original_present.load(std::memory_order_acquire);
    if (original == nullptr) {
        return E_FAIL;
    }
    return original(swap_chain, sync_interval, flags);
}

HRESULT STDMETHODCALLTYPE hooked_present1(
    IDXGISwapChain1* swap_chain,
    UINT sync_interval,
    UINT flags,
    const DXGI_PRESENT_PARAMETERS* parameters) {
    PresentDispatchScope dispatch;
    if (dispatch.should_process()) {
        process_frame(swap_chain);
        observe_postprocessed_frame(swap_chain);
    }
    if (!g_present1_runtime_audited.exchange(true, std::memory_order_acq_rel)) {
        log_present_entry(
            "first-runtime-call",
            "Present1",
            g_present1_vtable_entry.load(std::memory_order_acquire),
            reinterpret_cast<void*>(&hooked_present1),
            reinterpret_cast<void*>(
                g_original_present1.load(std::memory_order_acquire)));
    }
    const Present1Function original =
        g_original_present1.load(std::memory_order_acquire);
    if (original == nullptr) {
        return E_FAIL;
    }
    return original(swap_chain, sync_interval, flags, parameters);
}

HRESULT STDMETHODCALLTYPE hooked_resize_buffers(
    IDXGISwapChain* swap_chain,
    UINT buffer_count,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT flags) {
    prepare_for_resize(
        swap_chain, buffer_count, width, height, format, flags);
    prepare_steam_screenshot_resize();
    const ResizeBuffersFunction original =
        g_original_resize_buffers.load(std::memory_order_acquire);
    if (original == nullptr) {
        return E_FAIL;
    }
    const HRESULT result = original(
        swap_chain, buffer_count, width, height, format, flags);
    report_resize_result(swap_chain, result);
    return result;
}
}
