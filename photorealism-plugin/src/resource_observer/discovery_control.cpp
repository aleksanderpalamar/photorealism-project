#include "resource_observer.hpp"

#include "../runtime.hpp"
#include "discovery.hpp"
#include "observer_lock.hpp"
#include "observer_state.hpp"

#include <windows.h>

namespace photorealism {

using namespace observer;

void update_backbuffer_signature(
    UINT width, UINT height, DXGI_FORMAT format) {
    if (width == 0 || height == 0) {
        return;
    }
    if (g_backbuffer_width.load(std::memory_order_acquire) == width &&
        g_backbuffer_height.load(std::memory_order_acquire) == height &&
        g_backbuffer_format.load(std::memory_order_acquire) ==
            static_cast<UINT>(format)) {
        return;
    }

    AcquireSRWLockExclusive(&g_observer_lock);
    if (g_backbuffer_width.load(std::memory_order_relaxed) != width ||
        g_backbuffer_height.load(std::memory_order_relaxed) != height ||
        g_backbuffer_format.load(std::memory_order_relaxed) !=
            static_cast<UINT>(format)) {
        start_discovery(
            width,
            height,
            static_cast<UINT>(format),
            "iniciada por backbuffer");
    }
    ReleaseSRWLockExclusive(&g_observer_lock);
}

void restart_depth_discovery() {
    const UINT width = g_backbuffer_width.load(std::memory_order_acquire);
    const UINT height = g_backbuffer_height.load(std::memory_order_acquire);
    const UINT format = g_backbuffer_format.load(std::memory_order_acquire);
    if (width == 0 || height == 0) {
        return;
    }

    AcquireSRWLockExclusive(&g_observer_lock);
    start_discovery(width, height, format, "reiniciada via End");
    ReleaseSRWLockExclusive(&g_observer_lock);
}

void restart_depth_discovery_for_device_change() {
    const UINT width = g_backbuffer_width.load(std::memory_order_acquire);
    const UINT height = g_backbuffer_height.load(std::memory_order_acquire);
    const UINT format = g_backbuffer_format.load(std::memory_order_acquire);
    if (width == 0 || height == 0) {
        return;
    }

    AcquireSRWLockExclusive(&g_observer_lock);
    start_discovery(
        width,
        height,
        format,
        "reiniciada por troca de dispositivo");
    ReleaseSRWLockExclusive(&g_observer_lock);
}

}
