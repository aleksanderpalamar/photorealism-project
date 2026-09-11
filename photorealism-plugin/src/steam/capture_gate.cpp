#include "capture_gate.hpp"

#include "request_gate.hpp"

#include <windows.h>

namespace photorealism {
namespace steam {

std::atomic<std::uint32_t> g_requests{0};
std::atomic<bool> g_callback_accepting{false};
std::atomic<bool> g_capture_cycle_active{false};
std::atomic<std::uint64_t> g_last_accepted_tick{0};
std::atomic<std::uint64_t> g_accepted_requests{0};
std::atomic<std::uint64_t> g_coalesced_requests{0};
std::atomic<std::uint64_t> g_completed_writes{0};

namespace {

class ScreenshotRequestedCallback final : public CallbackBase {
  public:
    ScreenshotRequestedCallback() {
        callback = kScreenshotRequestedCallback;
    }

    void Run(void*) override {
        if (!g_callback_accepting.load(std::memory_order_acquire)) {
            return;
        }
        const std::uint64_t now = GetTickCount64();
        bool expected = false;
        if (!g_capture_cycle_active.compare_exchange_strong(
                expected,
                true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            g_coalesced_requests.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const std::uint64_t previous =
            g_last_accepted_tick.load(std::memory_order_acquire);
        if (steam_screenshot::request_is_duplicate(now, previous, false)) {
            g_capture_cycle_active.store(false, std::memory_order_release);
            g_coalesced_requests.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        g_last_accepted_tick.store(now, std::memory_order_release);
        g_accepted_requests.fetch_add(1, std::memory_order_relaxed);
        g_requests.store(1u, std::memory_order_release);
    }

    void Run(void*, bool, std::uint64_t) override {}

    int GetCallbackSizeBytes() override { return 1; }
};

ScreenshotRequestedCallback g_screenshot_callback;
static_assert(
    sizeof(ScreenshotRequestedCallback) == 16,
    "Steam CCallbackBase ABI layout changed for Windows x64");

}

CallbackBase* screenshot_callback() {
    return &g_screenshot_callback;
}

std::uint64_t accepted_requests() {
    return g_accepted_requests.load(std::memory_order_relaxed);
}

std::uint64_t coalesced_requests() {
    return g_coalesced_requests.load(std::memory_order_relaxed);
}

void reset_gate() {
    g_requests.store(0, std::memory_order_release);
    g_capture_cycle_active.store(false, std::memory_order_release);
}

}
}
