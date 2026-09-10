#include "discovery.hpp"

#include "../depth_scoring.hpp"
#include "observer_lock.hpp"
#include "observer_state.hpp"

#include <windows.h>

namespace photorealism {
namespace observer {
namespace {

bool discovery_window_closed(ULONGLONG now, DiscoveryScan* scan) {
    const ULONGLONG elapsed = now - g_discovery_started_at;
    bool confident_candidate_observed = false;
    if (elapsed >= kEarlyDiscoveryMinimumMilliseconds) {
        const UINT backbuffer_width =
            g_backbuffer_width.load(std::memory_order_relaxed);
        const UINT backbuffer_height =
            g_backbuffer_height.load(std::memory_order_relaxed);
        for (const ObservedDepthResource& entry : g_observed_resources) {
            if (entry.identity != nullptr &&
                depth_scoring::is_scene_candidate(
                    entry.width,
                    entry.height,
                    static_cast<std::uint64_t>(entry.bindings),
                    entry.sample_count,
                    backbuffer_width,
                    backbuffer_height,
                    elapsed)) {
                confident_candidate_observed = true;
                break;
            }
        }
    }
    if (!confident_candidate_observed &&
        elapsed < kDiscoveryDurationMilliseconds) {
        return false;
    }

    g_discovery_active.store(false, std::memory_order_release);
    scan->elapsed = elapsed;
    scan->early_confidence = confident_candidate_observed;
    return true;
}

void build_snapshots(DiscoveryScan* scan) {
    scan->backbuffer_width = g_backbuffer_width.load(std::memory_order_relaxed);
    scan->backbuffer_height =
        g_backbuffer_height.load(std::memory_order_relaxed);

    for (const ObservedDepthResource& entry : g_observed_resources) {
        if (entry.identity == nullptr) {
            continue;
        }
        ResourceSnapshot& snapshot = scan->resources[scan->resource_count++];
        snapshot.identity = entry.identity;
        snapshot.texture = entry.texture;
        snapshot.description = entry.description;
        snapshot.width = entry.width;
        snapshot.height = entry.height;
        snapshot.texture_format = entry.texture_format;
        snapshot.view_format = entry.view_format;
        snapshot.sample_count = entry.sample_count;
        snapshot.bind_flags = entry.bind_flags;
        snapshot.bindings = static_cast<std::uint64_t>(entry.bindings);
        snapshot.observed_views = entry.observed_views;
        snapshot.score = depth_scoring::resource_score(
            snapshot.width,
            snapshot.height,
            snapshot.bindings,
            scan->backbuffer_width,
            scan->backbuffer_height);
    }
}

void select_candidate(DiscoveryScan* scan) {
    scan->selected_index = kInvalidResourceIndex;
    for (UINT index = 0; index < scan->resource_count; ++index) {
        if (!depth_scoring::is_scene_candidate(
                scan->resources[index].width,
                scan->resources[index].height,
                scan->resources[index].bindings,
                scan->resources[index].sample_count,
                scan->backbuffer_width,
                scan->backbuffer_height,
                scan->elapsed)) {
            continue;
        }
        if (scan->selected_index == kInvalidResourceIndex ||
            scan->resources[index].score > scan->resources[scan->selected_index].score) {
            scan->selected_index = index;
        }
    }
    scan->candidate_retained =
        scan->selected_index != kInvalidResourceIndex &&
        scan->resources[scan->selected_index].texture != nullptr;
    if (scan->candidate_retained) {
        ResourceSnapshot& selected = scan->resources[scan->selected_index];
        selected.texture->AddRef();
        g_retained_candidate = selected.texture;
        g_retained_candidate_identity = selected.identity;
        g_retained_candidate_description = selected.description;
        g_depth_candidate_finalized = true;
        g_depth_candidate_binding_serial.store(1, std::memory_order_release);
    } else {
        if (g_retained_candidate != nullptr) {
            g_retained_candidate->Release();
            g_retained_candidate = nullptr;
        }
        g_retained_candidate_identity = nullptr;
        g_retained_candidate_description = {};
        g_depth_candidate_finalized = false;
    }
    scan->selected_generation = g_depth_candidate_generation;
}

void build_groups(DiscoveryScan* scan) {
    for (UINT resource_index = 0;
         resource_index < scan->resource_count;
         ++resource_index) {
        const ResourceSnapshot& resource = scan->resources[resource_index];
        UINT group_index = kInvalidResourceIndex;
        for (UINT index = 0; index < scan->group_count; ++index) {
            if (same_group(scan->groups[index], resource)) {
                group_index = index;
                break;
            }
        }
        if (group_index == kInvalidResourceIndex &&
            scan->group_count < kMaximumObservedResources) {
            group_index = scan->group_count++;
            scan->groups[group_index].width = resource.width;
            scan->groups[group_index].height = resource.height;
            scan->groups[group_index].texture_format = resource.texture_format;
            scan->groups[group_index].view_format = resource.view_format;
            scan->groups[group_index].sample_count = resource.sample_count;
            scan->groups[group_index].bind_flags = resource.bind_flags;
        }
        if (group_index != kInvalidResourceIndex) {
            ResolutionGroup& group = scan->groups[group_index];
            ++group.resource_count;
            group.bindings += resource.bindings;
            group.score += resource.score;
        }
    }
}

}

bool run_discovery_pass(DiscoveryScan* scan) {
    const ULONGLONG now = GetTickCount64();
    if (!discovery_window_closed(now, scan)) {
        return false;
    }

    build_snapshots(scan);
    select_candidate(scan);
    build_groups(scan);

    scan->resource_evictions = g_resource_evictions;
    scan->view_replacements = g_view_cache_replacements;
    scan->completed_cycle = g_discovery_cycle;
    clear_observation_catalog();
    if (!scan->candidate_retained) {
        g_discovery_started_at = now;
        ++g_discovery_cycle;
        g_discovery_active.store(true, std::memory_order_release);
    }
    return true;
}

}
}
