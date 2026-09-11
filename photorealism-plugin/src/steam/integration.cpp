#include "integration.hpp"

#include "../runtime.hpp"
#include "capture_gate.hpp"
#include "capture_slots.hpp"
#include "conversion_worker.hpp"
#include "steam_screenshots.hpp"

#include <windows.h>

namespace photorealism {
namespace steam {
namespace {

constexpr unsigned kMaximumAttempts = 30u;
constexpr ULONGLONG kAttemptIntervalMilliseconds = 1000u;

ScreenshotsApi g_api = {};
bool g_integration_attempted = false;
unsigned g_integration_attempt_count = 0;
ULONGLONG g_last_integration_attempt = 0;
bool g_callback_registered = false;
bool g_custom_capture_active = false;
bool g_custom_capture_failed = false;

bool attempt_is_due() {
    const ULONGLONG now = GetTickCount64();
    const bool too_soon =
        g_last_integration_attempt != 0 &&
        now - g_last_integration_attempt < kAttemptIntervalMilliseconds;
    if (too_soon) {
        return false;
    }
    g_last_integration_attempt = now;
    ++g_integration_attempt_count;
    return true;
}

void give_up(const char* message) {
    g_integration_attempted = true;
    log_message("%s", message);
}

bool handle_module_absent() {
    if (g_integration_attempt_count < kMaximumAttempts) {
        return false;
    }
    give_up(
        "Steam screenshot 0.10.4 indisponivel apos espera limitada; "
        "overlay nativo permanece responsavel pelo F12.");
    return false;
}

bool handle_exports_incomplete() {
    if (g_integration_attempt_count < kMaximumAttempts) {
        return false;
    }
    give_up(
        "Steamworks screenshots v003 incompleto ou ainda nao inicializado; "
        "HookScreenshots nao ativado e F12 nativo preservado.");
    return false;
}

bool activate_hook() {
    g_api.register_callback(
        screenshot_callback(), kScreenshotRequestedCallback);
    g_callback_registered = true;
    g_api.hook(true);

    const bool refused =
        g_api.is_hooked != nullptr && !g_api.is_hooked(g_api.screenshots);
    if (!refused) {
        return true;
    }
    g_api.unregister_callback(screenshot_callback());
    g_callback_registered = false;
    log_message("Steam recusou HookScreenshots; F12 nativo preservado.");
    return false;
}

}

const ScreenshotsApi& api() {
    return g_api;
}

bool ensure_integration() {
    if (g_custom_capture_active) {
        return true;
    }
    if (g_integration_attempted || g_custom_capture_failed) {
        return false;
    }
    if (!attempt_is_due()) {
        return false;
    }

    const LoadResult load = load_screenshots_api(&g_api);
    if (load == LoadResult::module_absent) {
        return handle_module_absent();
    }
    if (load == LoadResult::exports_incomplete) {
        return handle_exports_incomplete();
    }
    if (!start_conversion_worker()) {
        give_up(
            "Worker de screenshot Steam indisponivel; HookScreenshots nao "
            "ativado e F12 nativo preservado.");
        return false;
    }
    if (!activate_hook()) {
        return false;
    }

    g_callback_accepting.store(true, std::memory_order_release);
    g_custom_capture_active = true;
    g_integration_attempted = true;
    log_message(
        "Steam screenshot 0.10.4 ativo via ISteamScreenshots v003: "
        "ScreenshotRequested_t=2302, readback assincrono e exatamente um "
        "WriteScreenshot por toque; deduplicacao=750ms e nenhuma tecla "
        "interceptada.");
    return true;
}

void return_to_native_capture(const char* reason, bool trigger) {
    if (!g_custom_capture_active) {
        return;
    }
    g_callback_accepting.store(false, std::memory_order_release);
    g_api.hook(false);
    if (g_callback_registered) {
        g_api.unregister_callback(screenshot_callback());
        g_callback_registered = false;
    }
    g_custom_capture_active = false;
    g_custom_capture_failed = true;
    reset_gate();
    log_message(
        "Steam screenshot 0.10.4 retornou ao overlay nativo: result=failure "
        "reason=%s accepted=%llu coalesced=%llu.",
        reason,
        static_cast<unsigned long long>(accepted_requests()),
        static_cast<unsigned long long>(coalesced_requests()));
    if (trigger) {
        g_api.trigger();
    }
    shutdown_steam_screenshots();
}

}

void prepare_steam_screenshot_resize() {
    shutdown_steam_screenshots();
}

void shutdown_steam_screenshots() {
    using namespace steam;
    g_callback_accepting.store(false, std::memory_order_release);
    if (g_custom_capture_active) {
        g_api.hook(false);
    }
    if (g_callback_registered && g_api.unregister_callback != nullptr) {
        g_api.unregister_callback(screenshot_callback());
        g_callback_registered = false;
    }
    g_custom_capture_active = false;
    reset_gate();
    stop_conversion_worker();
    {
        BlockingCaptureLock lock;
        release_capture_slots_locked();
    }
    if (g_custom_capture_failed) {
        return;
    }
    g_integration_attempted = false;
    g_integration_attempt_count = 0;
    g_last_integration_attempt = 0;
}

}
