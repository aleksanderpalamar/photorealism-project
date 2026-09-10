#include "observer_state.hpp"

#include "../depth_scoring.hpp"

#include <cstring>

namespace photorealism {
namespace observer {

SRWLOCK g_observer_lock = SRWLOCK_INIT;
std::atomic<bool> g_discovery_active{false};
std::atomic<UINT> g_backbuffer_width{0};
std::atomic<UINT> g_backbuffer_height{0};
std::atomic<UINT> g_backbuffer_format{DXGI_FORMAT_UNKNOWN};
ULONGLONG g_discovery_started_at = 0;
volatile LONG g_observation_counter = 0;
UINT g_resource_evictions = 0;
UINT g_view_cache_replacements = 0;
ID3D11Texture2D* g_retained_candidate = nullptr;
void* g_retained_candidate_identity = nullptr;
D3D11_TEXTURE2D_DESC g_retained_candidate_description = {};
bool g_depth_candidate_finalized = false;
std::uint64_t g_depth_candidate_generation = 0;
std::atomic<std::uint64_t> g_depth_candidate_binding_serial{0};
std::uint64_t g_discovery_cycle = 0;
ObservedDepthResource g_observed_resources[kMaximumObservedResources] = {};
ViewCacheEntry g_view_cache[kViewCacheCapacity] = {};

UINT pointer_hash(const void* pointer, UINT capacity) {
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(pointer);
    return static_cast<UINT>(((value >> 4) ^ (value >> 13)) % capacity);
}

double aspect_error_percent(
    UINT width, UINT height, UINT reference_width, UINT reference_height) {
    if (width == 0 || height == 0 ||
        reference_width == 0 || reference_height == 0) {
        return 100.0;
    }
    const std::uint64_t left =
        static_cast<std::uint64_t>(width) * reference_height;
    const std::uint64_t right =
        static_cast<std::uint64_t>(height) * reference_width;
    const std::uint64_t difference = left > right ? left - right : right - left;
    const std::uint64_t scale = left > right ? left : right;
    return scale == 0
               ? 100.0
               : static_cast<double>(difference) * 100.0 /
                     static_cast<double>(scale);
}
void clear_depth_candidate() {
    if (g_retained_candidate != nullptr) {
        g_retained_candidate->Release();
        g_retained_candidate = nullptr;
    }
    g_retained_candidate_identity = nullptr;
    g_retained_candidate_description = {};
    g_depth_candidate_finalized = false;
    g_depth_candidate_binding_serial.store(0, std::memory_order_release);
    ++g_depth_candidate_generation;
    if (g_depth_candidate_generation == 0) {
        g_depth_candidate_generation = 1;
    }
}

void clear_observation_catalog() {
    for (ObservedDepthResource& entry : g_observed_resources) {
        if (entry.texture != nullptr) {
            entry.texture->Release();
        }
        entry = {};
        entry.generation = 1;
    }
    for (ViewCacheEntry& entry : g_view_cache) {
        entry = {};
        entry.resource_index = kInvalidResourceIndex;
    }
    InterlockedExchange(&g_observation_counter, 0);
    g_resource_evictions = 0;
    g_view_cache_replacements = 0;
}

void clear_observer_state() {
    clear_depth_candidate();
    clear_observation_catalog();
}
ViewCacheEntry* find_cached_view(ID3D11DepthStencilView* identity) {
    const UINT start = pointer_hash(identity, kViewCacheCapacity);
    for (UINT probe = 0; probe < kViewCacheProbeCount; ++probe) {
        ViewCacheEntry& entry =
            g_view_cache[(start + probe) % kViewCacheCapacity];
        if (entry.identity == identity) {
            return &entry;
        }
        if (entry.identity == nullptr) {
            return nullptr;
        }
    }
    return nullptr;
}

void cache_view(
    ID3D11DepthStencilView* identity,
    UINT resource_index,
    UINT resource_generation) {
    const UINT start = pointer_hash(identity, kViewCacheCapacity);
    ViewCacheEntry* destination = nullptr;
    for (UINT probe = 0; probe < kViewCacheProbeCount; ++probe) {
        ViewCacheEntry& entry =
            g_view_cache[(start + probe) % kViewCacheCapacity];
        if (entry.identity == identity || entry.identity == nullptr) {
            destination = &entry;
            break;
        }
    }
    if (destination == nullptr) {
        destination = &g_view_cache[start];
        ++g_view_cache_replacements;
    }
    destination->resource_index = resource_index;
    destination->resource_generation = resource_generation;
    destination->identity = identity;
}

UINT find_resource(void* identity) {
    for (UINT index = 0; index < kMaximumObservedResources; ++index) {
        if (g_observed_resources[index].identity == identity) {
            return index;
        }
    }
    return kInvalidResourceIndex;
}
UINT select_resource_slot(
    const D3D11_TEXTURE2D_DESC& description,
    UINT reference_width,
    UINT reference_height) {
    UINT free_index = kInvalidResourceIndex;
    UINT weakest_index = 0;
    std::uint64_t weakest_score = UINT64_MAX;
    for (UINT index = 0; index < kMaximumObservedResources; ++index) {
        const ObservedDepthResource& entry = g_observed_resources[index];
        if (entry.identity == nullptr) {
            free_index = index;
            break;
        }
        const std::uint64_t score = depth_scoring::resource_score(
            entry.width,
            entry.height,
            static_cast<std::uint64_t>(entry.bindings),
            reference_width,
            reference_height);
        if (score < weakest_score) {
            weakest_score = score;
            weakest_index = index;
        }
    }
    if (free_index != kInvalidResourceIndex) {
        return free_index;
    }

    const std::uint64_t incoming_score = depth_scoring::resource_score(
        description.Width,
        description.Height,
        1,
        reference_width,
        reference_height);
    if (incoming_score <= weakest_score) {
        return kInvalidResourceIndex;
    }

    ++g_resource_evictions;
    ObservedDepthResource& evicted = g_observed_resources[weakest_index];
    const UINT next_generation = evicted.generation + 1;
    if (evicted.texture != nullptr) {
        evicted.texture->Release();
    }
    evicted = {};
    evicted.generation = next_generation == 0 ? 1 : next_generation;
    return weakest_index;
}

bool same_group(
    const ResolutionGroup& group, const ResourceSnapshot& resource) {
    return group.width == resource.width &&
           group.height == resource.height &&
           group.texture_format == resource.texture_format &&
           group.view_format == resource.view_format &&
           group.sample_count == resource.sample_count &&
           group.bind_flags == resource.bind_flags;
}
}
}
