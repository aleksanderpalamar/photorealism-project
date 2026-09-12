#include "back_buffer_proxy.hpp"
#include "hook.hpp"

#include "../runtime.hpp"
#include "context_hooks.hpp"
#include "device_probe.hpp"
#include "hook_audit.hpp"
#include "hook_state.hpp"
#include "swap_chain_hooks.hpp"
#include "vtable_patch.hpp"

#include <dxgi1_2.h>

namespace photorealism {
namespace {

struct InstallReport {
    bool present;
    bool present1_available;
    bool present1;
    bool resize_buffers;
    bool render_targets;
    bool render_targets_and_uavs;
    bool clear_depth_stencil;
};

bool already_installed() {
    using namespace hook_state;
    const bool present1_ready =
        !g_present1_required.load(std::memory_order_acquire) ||
        g_original_present1.load(std::memory_order_acquire) != nullptr;
    return g_original_present.load(std::memory_order_acquire) != nullptr &&
           present1_ready &&
           g_original_resize_buffers.load(std::memory_order_acquire) != nullptr &&
           g_original_set_render_targets.load(std::memory_order_acquire) !=
               nullptr &&
           g_original_set_render_targets_and_uavs.load(
               std::memory_order_acquire) != nullptr &&
           g_original_clear_depth_stencil_view.load(
               std::memory_order_acquire) != nullptr;
}

void patch_present(IDXGISwapChain* swap_chain, InstallReport* report) {
    void** vtable = *reinterpret_cast<void***>(swap_chain);
    void** present_entry = &vtable[8];
    hook_state::g_present_vtable_entry.store(
        present_entry, std::memory_order_release);
    report->present = replace_vtable_entry(
        present_entry,
        reinterpret_cast<void*>(&hooked_present),
        &hook_state::g_original_present);
    report->resize_buffers = replace_vtable_entry(
        &vtable[13],
        reinterpret_cast<void*>(&hooked_resize_buffers),
        &hook_state::g_original_resize_buffers);
    patch_back_buffer_proxy(swap_chain);
}

void patch_present1(IDXGISwapChain* swap_chain, InstallReport* report) {
    IDXGISwapChain1* swap_chain1 = nullptr;
    if (FAILED(swap_chain->QueryInterface(
            IID_IDXGISwapChain1, reinterpret_cast<void**>(&swap_chain1))) ||
        swap_chain1 == nullptr) {
        return;
    }
    report->present1_available = true;
    hook_state::g_present1_required.store(true, std::memory_order_release);
    void** vtable = *reinterpret_cast<void***>(swap_chain1);
    void** present1_entry = &vtable[22];
    hook_state::g_present1_vtable_entry.store(
        present1_entry, std::memory_order_release);
    report->present1 = replace_vtable_entry(
        present1_entry,
        reinterpret_cast<void*>(&hooked_present1),
        &hook_state::g_original_present1);
    swap_chain1->Release();
}

void patch_context(ID3D11DeviceContext* context, InstallReport* report) {
    if (context == nullptr) {
        return;
    }
    void** vtable = *reinterpret_cast<void***>(context);
    report->render_targets = replace_vtable_entry(
        &vtable[33],
        reinterpret_cast<void*>(&hooked_set_render_targets),
        &hook_state::g_original_set_render_targets);
    report->render_targets_and_uavs = replace_vtable_entry(
        &vtable[34],
        reinterpret_cast<void*>(&hooked_set_render_targets_and_uavs),
        &hook_state::g_original_set_render_targets_and_uavs);
    report->clear_depth_stencil = replace_vtable_entry(
        &vtable[53],
        reinterpret_cast<void*>(&hooked_clear_depth_stencil_view),
        &hook_state::g_original_clear_depth_stencil_view);
}

bool report_is_complete(const InstallReport& report) {
    return report.present &&
           (!report.present1_available || report.present1) &&
           report.resize_buffers && report.render_targets &&
           report.render_targets_and_uavs && report.clear_depth_stencil;
}

void log_success(const InstallReport& report, D3D_FEATURE_LEVEL level) {
    log_message(
        "Hooks Present/Present1/ResizeBuffers/OMSetRenderTargets*/"
        "ClearDepthStencilView instalados; feature level=0x%X Present1=%s.",
        static_cast<unsigned>(level),
        report.present1_available
            ? (report.present1 ? "ativo-slot22" : "falha-slot22")
            : "indisponivel");
    log_present_entry(
        "install",
        "Present",
        hook_state::g_present_vtable_entry.load(std::memory_order_acquire),
        reinterpret_cast<void*>(&hooked_present),
        reinterpret_cast<void*>(
            hook_state::g_original_present.load(std::memory_order_acquire)));
    if (!report.present1_available) {
        return;
    }
    log_present_entry(
        "install",
        "Present1",
        hook_state::g_present1_vtable_entry.load(std::memory_order_acquire),
        reinterpret_cast<void*>(&hooked_present1),
        reinterpret_cast<void*>(
            hook_state::g_original_present1.load(std::memory_order_acquire)));
}

void log_partial_failure(const InstallReport& report) {
    log_message(
        "Falha parcial nos hooks: Present=%s Present1=%s ResizeBuffers=%s "
        "OMSetRenderTargets=%s OMSetRenderTargetsAndUAVs=%s "
        "ClearDepthStencilView=%s.",
        report.present ? "ok" : "falha",
        report.present1_available ? (report.present1 ? "ok" : "falha")
                                  : "indisponivel",
        report.resize_buffers ? "ok" : "falha",
        report.render_targets ? "ok" : "falha",
        report.render_targets_and_uavs ? "ok" : "falha",
        report.clear_depth_stencil ? "ok" : "falha");
}

}

bool install_swap_chain_hooks() {
    if (already_installed()) {
        return true;
    }

    DeviceProbe probe;
    if (!probe.create()) {
        return false;
    }

    InstallReport report = {};
    patch_present(probe.swap_chain(), &report);
    patch_present1(probe.swap_chain(), &report);
    patch_context(probe.context(), &report);

    if (!report_is_complete(report)) {
        log_partial_failure(report);
        return false;
    }
    log_success(report, probe.feature_level());
    return true;
}

}
