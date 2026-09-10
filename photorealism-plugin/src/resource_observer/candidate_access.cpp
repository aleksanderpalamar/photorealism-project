#include "resource_observer.hpp"

#include "../runtime.hpp"
#include "discovery.hpp"
#include "observer_lock.hpp"
#include "observer_state.hpp"

#include <windows.h>

namespace photorealism {

using namespace observer;

bool acquire_depth_candidate(
    ID3D11Texture2D** texture,
    D3D11_TEXTURE2D_DESC* description,
    std::uint64_t* generation,
    std::uint64_t* binding_serial) {
    if (texture == nullptr || description == nullptr || generation == nullptr ||
        binding_serial == nullptr) {
        return false;
    }
    *texture = nullptr;
    *description = {};
    *generation = 0;
    *binding_serial = 0;

    AcquireSRWLockShared(&g_observer_lock);
    const bool available =
        g_depth_candidate_finalized && g_retained_candidate != nullptr;
    if (available) {
        g_retained_candidate->AddRef();
        *texture = g_retained_candidate;
        *description = g_retained_candidate_description;
        *generation = g_depth_candidate_generation;
        *binding_serial = g_depth_candidate_binding_serial.load(
            std::memory_order_acquire);
    }
    ReleaseSRWLockShared(&g_observer_lock);
    return available;
}

bool invalidate_stale_depth_candidate(
    std::uint64_t generation,
    std::uint64_t binding_serial) {
    const UINT width = g_backbuffer_width.load(std::memory_order_acquire);
    const UINT height = g_backbuffer_height.load(std::memory_order_acquire);
    const UINT format = g_backbuffer_format.load(std::memory_order_acquire);
    if (width == 0 || height == 0 || generation == 0) {
        return false;
    }

    bool invalidated = false;
    AcquireSRWLockExclusive(&g_observer_lock);
    if (!g_discovery_active.load(std::memory_order_relaxed) &&
        g_depth_candidate_finalized &&
        g_depth_candidate_generation == generation &&
        g_depth_candidate_binding_serial.load(std::memory_order_relaxed) ==
            binding_serial) {
        start_discovery(
            width,
            height,
            format,
            "reiniciada automaticamente por depth obsoleto");
        invalidated = true;
    }
    ReleaseSRWLockExclusive(&g_observer_lock);
    return invalidated;
}

}
