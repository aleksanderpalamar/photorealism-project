#pragma once

#include "types.hpp"

namespace photorealism {
namespace observer {

extern SRWLOCK g_observer_lock;
extern std::atomic<bool> g_discovery_active;
extern std::atomic<UINT> g_backbuffer_width;
extern std::atomic<UINT> g_backbuffer_height;
extern std::atomic<UINT> g_backbuffer_format;
extern ULONGLONG g_discovery_started_at;
extern volatile LONG g_observation_counter;
extern UINT g_resource_evictions;
extern UINT g_view_cache_replacements;
extern ID3D11Texture2D* g_retained_candidate;
extern void* g_retained_candidate_identity;
extern D3D11_TEXTURE2D_DESC g_retained_candidate_description;
extern bool g_depth_candidate_finalized;
extern std::uint64_t g_depth_candidate_generation;
extern std::atomic<std::uint64_t> g_depth_candidate_binding_serial;
extern std::uint64_t g_discovery_cycle;
extern ObservedDepthResource g_observed_resources[kMaximumObservedResources];
extern ViewCacheEntry g_view_cache[kViewCacheCapacity];

UINT pointer_hash(const void* pointer, UINT capacity);
double aspect_error_percent(
    UINT width, UINT height, UINT reference_width, UINT reference_height);
void clear_depth_candidate();
void clear_observation_catalog();
void clear_observer_state();
ViewCacheEntry* find_cached_view(ID3D11DepthStencilView* identity);
void cache_view(
    ID3D11DepthStencilView* identity,
    UINT resource_index,
    UINT resource_generation);
UINT find_resource(void* identity);
UINT select_resource_slot(
    const D3D11_TEXTURE2D_DESC& description,
    UINT reference_width,
    UINT reference_height);
bool same_group(
    const ResolutionGroup& group, const ResourceSnapshot& resource);

}
}
