#include "discovery.hpp"

#include "../runtime.hpp"
#include "observer_lock.hpp"
#include "observer_state.hpp"

#include <windows.h>

namespace photorealism {
namespace observer {

void finish_discovery_if_due() {
    if (!g_discovery_active.load(std::memory_order_acquire)) {
        return;
    }
    const LONG observation = InterlockedIncrement(&g_observation_counter);
    if ((observation & 0xFF) != 0) {
        return;
    }

    DiscoveryScan scan = {};
    {
        WriteLock lock;
        if (!g_discovery_active.load(std::memory_order_relaxed)) {
            return;
        }
        if (!run_discovery_pass(&scan)) {
            return;
        }
    }
    report_discovery(scan);
}

void start_discovery(
    UINT width, UINT height, UINT format, const char* reason) {
    clear_observer_state();
    g_backbuffer_width.store(width, std::memory_order_release);
    g_backbuffer_height.store(height, std::memory_order_release);
    g_backbuffer_format.store(format, std::memory_order_release);
    g_discovery_started_at = GetTickCount64();
    ++g_discovery_cycle;
    g_discovery_active.store(true, std::memory_order_release);
    log_message(
        "Descoberta depth 0.10.1 %s: cycle=%llu backbuffer=%ux%u format=%u "
        "janela=%llums early=%llums resources=%u view_cache=%u.",
        reason,
        static_cast<unsigned long long>(g_discovery_cycle),
        width,
        height,
        format,
        static_cast<unsigned long long>(kDiscoveryDurationMilliseconds),
        static_cast<unsigned long long>(kEarlyDiscoveryMinimumMilliseconds),
        kMaximumObservedResources,
        kViewCacheCapacity);
}
}
}
