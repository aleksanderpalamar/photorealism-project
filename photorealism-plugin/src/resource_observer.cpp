#include "resource_observer.hpp"

#include "depth_scoring.hpp"
#include "runtime.hpp"

#include <windows.h>

#include <atomic>
#include <cstdint>

namespace photorealism {
namespace {

constexpr UINT kMaximumObservedResources = 256;
constexpr UINT kViewCacheCapacity = 4096;
constexpr UINT kViewCacheProbeCount = 32;
constexpr UINT kMaximumReportedGroups = 12;
constexpr UINT kMaximumReportedResources = 8;
constexpr UINT kInvalidResourceIndex = 0xFFFFFFFFu;
constexpr ULONGLONG kDiscoveryDurationMilliseconds = 30000;
constexpr ULONGLONG kEarlyDiscoveryMinimumMilliseconds = 3000;

struct ObservedDepthResource {
    void* identity;
    ID3D11Texture2D* texture;
    D3D11_TEXTURE2D_DESC description;
    UINT width;
    UINT height;
    DXGI_FORMAT texture_format;
    DXGI_FORMAT view_format;
    UINT sample_count;
    UINT bind_flags;
    volatile LONG64 bindings;
    UINT observed_views;
    UINT generation;
};

struct ViewCacheEntry {
    ID3D11DepthStencilView* identity;
    UINT resource_index;
    UINT resource_generation;
};

struct ResourceSnapshot {
    void* identity;
    ID3D11Texture2D* texture;
    D3D11_TEXTURE2D_DESC description;
    UINT width;
    UINT height;
    DXGI_FORMAT texture_format;
    DXGI_FORMAT view_format;
    UINT sample_count;
    UINT bind_flags;
    std::uint64_t bindings;
    UINT observed_views;
    std::uint64_t score;
};

struct ResolutionGroup {
    UINT width;
    UINT height;
    DXGI_FORMAT texture_format;
    DXGI_FORMAT view_format;
    UINT sample_count;
    UINT bind_flags;
    UINT resource_count;
    std::uint64_t bindings;
    std::uint64_t score;
};

ObservedDepthResource g_observed_resources[kMaximumObservedResources] = {};
ViewCacheEntry g_view_cache[kViewCacheCapacity] = {};
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

const char* format_name(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_R32G8X24_TYPELESS:
            return "R32G8X24_TYPELESS";
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            return "D32_FLOAT_S8X24_UINT";
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
            return "R32_FLOAT_X8X24_TYPELESS";
        case DXGI_FORMAT_R32_TYPELESS:
            return "R32_TYPELESS";
        case DXGI_FORMAT_D32_FLOAT:
            return "D32_FLOAT";
        case DXGI_FORMAT_R32_FLOAT:
            return "R32_FLOAT";
        case DXGI_FORMAT_R24G8_TYPELESS:
            return "R24G8_TYPELESS";
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return "D24_UNORM_S8_UINT";
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
            return "R24_UNORM_X8_TYPELESS";
        case DXGI_FORMAT_R16_TYPELESS:
            return "R16_TYPELESS";
        case DXGI_FORMAT_D16_UNORM:
            return "D16_UNORM";
        case DXGI_FORMAT_R16_UNORM:
            return "R16_UNORM";
        default:
            return "OTHER";
    }
}

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

void finish_discovery_if_due() {
    if (!g_discovery_active.load(std::memory_order_acquire)) {
        return;
    }
    const LONG observation = InterlockedIncrement(&g_observation_counter);
    if ((observation & 0xFF) != 0) {
        return;
    }

    const ULONGLONG now = GetTickCount64();
    AcquireSRWLockExclusive(&g_observer_lock);
    if (!g_discovery_active.load(std::memory_order_relaxed)) {
        ReleaseSRWLockExclusive(&g_observer_lock);
        return;
    }

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
        ReleaseSRWLockExclusive(&g_observer_lock);
        return;
    }

    g_discovery_active.store(false, std::memory_order_release);
    const UINT backbuffer_width =
        g_backbuffer_width.load(std::memory_order_relaxed);
    const UINT backbuffer_height =
        g_backbuffer_height.load(std::memory_order_relaxed);

    ResourceSnapshot resources[kMaximumObservedResources] = {};
    UINT resource_count = 0;
    for (const ObservedDepthResource& entry : g_observed_resources) {
        if (entry.identity == nullptr) {
            continue;
        }
        ResourceSnapshot& snapshot = resources[resource_count++];
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
            backbuffer_width,
            backbuffer_height);
    }

    UINT selected_resource_index = kInvalidResourceIndex;
    for (UINT index = 0; index < resource_count; ++index) {
        if (!depth_scoring::is_scene_candidate(
                resources[index].width,
                resources[index].height,
                resources[index].bindings,
                resources[index].sample_count,
                backbuffer_width,
                backbuffer_height,
                elapsed)) {
            continue;
        }
        if (selected_resource_index == kInvalidResourceIndex ||
            resources[index].score > resources[selected_resource_index].score) {
            selected_resource_index = index;
        }
    }
    const bool candidate_retained = selected_resource_index !=
                                        kInvalidResourceIndex &&
                                    resources[selected_resource_index].texture !=
                                        nullptr;
    if (candidate_retained) {
        ResourceSnapshot& selected = resources[selected_resource_index];
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
    const std::uint64_t selected_generation = g_depth_candidate_generation;

    ResolutionGroup groups[kMaximumObservedResources] = {};
    UINT group_count = 0;
    for (UINT resource_index = 0;
         resource_index < resource_count;
         ++resource_index) {
        const ResourceSnapshot& resource = resources[resource_index];
        UINT group_index = kInvalidResourceIndex;
        for (UINT index = 0; index < group_count; ++index) {
            if (same_group(groups[index], resource)) {
                group_index = index;
                break;
            }
        }
        if (group_index == kInvalidResourceIndex &&
            group_count < kMaximumObservedResources) {
            group_index = group_count++;
            groups[group_index].width = resource.width;
            groups[group_index].height = resource.height;
            groups[group_index].texture_format = resource.texture_format;
            groups[group_index].view_format = resource.view_format;
            groups[group_index].sample_count = resource.sample_count;
            groups[group_index].bind_flags = resource.bind_flags;
        }
        if (group_index != kInvalidResourceIndex) {
            ResolutionGroup& group = groups[group_index];
            ++group.resource_count;
            group.bindings += resource.bindings;
            group.score += resource.score;
        }
    }
    const UINT resource_evictions = g_resource_evictions;
    const UINT view_replacements = g_view_cache_replacements;
    const std::uint64_t completed_cycle = g_discovery_cycle;
    clear_observation_catalog();
    if (!candidate_retained) {
        g_discovery_started_at = now;
        ++g_discovery_cycle;
        g_discovery_active.store(true, std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&g_observer_lock);

    log_message(
        "Descoberta depth 0.10.1 concluida: cycle=%llu mode=%s "
        "resources=%u grupos=%u "
        "resource_evictions=%u view_cache_replacements=%u janela=%llums "
        "minimum_bindings=%llu minimum_rate=%llu/s scaled_area=%llu%%.",
        static_cast<unsigned long long>(completed_cycle),
        confident_candidate_observed ? "early-confidence" : "window-timeout",
        resource_count,
        group_count,
        resource_evictions,
        view_replacements,
        static_cast<unsigned long long>(kDiscoveryDurationMilliseconds),
        static_cast<unsigned long long>(
            depth_scoring::kMinimumCandidateBindings),
        static_cast<unsigned long long>(
            depth_scoring::kMinimumSceneBindingsPerSecond),
        static_cast<unsigned long long>(
            depth_scoring::kMinimumScaledSceneAreaPercent));

    if (candidate_retained) {
        const ResourceSnapshot& selected =
            resources[selected_resource_index];
        log_message(
            "Depth principal 0.10.1 selecionado automaticamente: "
            "resource=%p generation=%llu "
            "size=%ux%u texture_format=%u(%s) samples=%u "
            "bind_flags=0x%08X bindings=%llu score=%llu.",
            selected.identity,
            static_cast<unsigned long long>(selected_generation),
            selected.width,
            selected.height,
            static_cast<unsigned>(selected.texture_format),
            format_name(selected.texture_format),
            selected.sample_count,
            selected.bind_flags,
            static_cast<unsigned long long>(selected.bindings),
            static_cast<unsigned long long>(selected.score));
    } else {
        log_message(
            "Depth principal 0.10.1 ainda nao foi retido com seguranca; "
            "nenhum recurso elegivel atingiu a confianca minima e a "
            "descoberta continuara automaticamente no cycle=%llu.",
            static_cast<unsigned long long>(completed_cycle + 1));
    }

    bool reported_groups[kMaximumObservedResources] = {};
    const UINT group_report_count = group_count < kMaximumReportedGroups
                                        ? group_count
                                        : kMaximumReportedGroups;
    for (UINT rank = 0; rank < group_report_count; ++rank) {
        UINT best = kInvalidResourceIndex;
        for (UINT index = 0; index < group_count; ++index) {
            if (reported_groups[index]) {
                continue;
            }
            if (best == kInvalidResourceIndex ||
                groups[index].score > groups[best].score) {
                best = index;
            }
        }
        if (best == kInvalidResourceIndex) {
            break;
        }
        reported_groups[best] = true;
        const ResolutionGroup& group = groups[best];
        const double area_scale = backbuffer_width == 0 || backbuffer_height == 0
                                      ? 0.0
                                      : static_cast<double>(group.width) *
                                            static_cast<double>(group.height) /
                                            (static_cast<double>(backbuffer_width) *
                                             static_cast<double>(backbuffer_height));
        log_message(
            "Depth grupo #%u: size=%ux%u area_scale=%.3fx "
            "aspect_error=%.2f%% texture_format=%u(%s) view_format=%u(%s) "
            "samples=%u bind_flags=0x%08X shader_readable=%s "
            "resources=%u bindings=%llu score=%llu.",
            rank + 1,
            group.width,
            group.height,
            area_scale,
            aspect_error_percent(
                group.width,
                group.height,
                backbuffer_width,
                backbuffer_height),
            static_cast<unsigned>(group.texture_format),
            format_name(group.texture_format),
            static_cast<unsigned>(group.view_format),
            format_name(group.view_format),
            group.sample_count,
            group.bind_flags,
            (group.bind_flags & D3D11_BIND_SHADER_RESOURCE) != 0
                ? "sim"
                : "nao",
            group.resource_count,
            static_cast<unsigned long long>(group.bindings),
            static_cast<unsigned long long>(group.score));
    }

    bool reported_resources[kMaximumObservedResources] = {};
    const UINT resource_report_count =
        resource_count < kMaximumReportedResources
            ? resource_count
            : kMaximumReportedResources;
    for (UINT rank = 0; rank < resource_report_count; ++rank) {
        UINT best = kInvalidResourceIndex;
        for (UINT index = 0; index < resource_count; ++index) {
            if (reported_resources[index]) {
                continue;
            }
            if (best == kInvalidResourceIndex ||
                resources[index].score > resources[best].score) {
                best = index;
            }
        }
        if (best == kInvalidResourceIndex) {
            break;
        }
        reported_resources[best] = true;
        const ResourceSnapshot& resource = resources[best];
        log_message(
            "Depth recurso #%u: resource=%p size=%ux%u "
            "texture_format=%u(%s) view_format=%u(%s) samples=%u "
            "bind_flags=0x%08X shader_readable=%s views=%u bindings=%llu "
            "score=%llu.",
            rank + 1,
            resource.identity,
            resource.width,
            resource.height,
            static_cast<unsigned>(resource.texture_format),
            format_name(resource.texture_format),
            static_cast<unsigned>(resource.view_format),
            format_name(resource.view_format),
            resource.sample_count,
            resource.bind_flags,
            (resource.bind_flags & D3D11_BIND_SHADER_RESOURCE) != 0
                ? "sim"
                : "nao",
            resource.observed_views,
            static_cast<unsigned long long>(resource.bindings),
            static_cast<unsigned long long>(resource.score));
    }
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

}  // namespace

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

void observe_depth_activity(
    ID3D11DeviceContext* context,
    ID3D11DepthStencilView* depth_target) {
    if (context == nullptr || depth_target == nullptr ||
        context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
        return;
    }

    AcquireSRWLockShared(&g_observer_lock);
    if (!g_depth_candidate_finalized) {
        ReleaseSRWLockShared(&g_observer_lock);
        return;
    }
    ViewCacheEntry* cached = find_cached_view(depth_target);
    if (cached != nullptr &&
        cached->resource_index < kMaximumObservedResources) {
        ObservedDepthResource& resource =
            g_observed_resources[cached->resource_index];
        if (resource.identity != nullptr &&
            resource.generation == cached->resource_generation &&
            resource.identity == g_retained_candidate_identity) {
            g_depth_candidate_binding_serial.fetch_add(
                1, std::memory_order_release);
            ReleaseSRWLockShared(&g_observer_lock);
            return;
        }
    }
    ReleaseSRWLockShared(&g_observer_lock);

    ID3D11Resource* resource_interface = nullptr;
    ID3D11Texture2D* texture = nullptr;
    depth_target->GetResource(&resource_interface);
    if (resource_interface == nullptr || FAILED(resource_interface->QueryInterface(
            IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture))) ||
        texture == nullptr) {
        if (resource_interface != nullptr) {
            resource_interface->Release();
        }
        return;
    }

    AcquireSRWLockShared(&g_observer_lock);
    if (g_depth_candidate_finalized &&
        static_cast<void*>(texture) == g_retained_candidate_identity) {
        g_depth_candidate_binding_serial.fetch_add(
            1, std::memory_order_release);
    }
    ReleaseSRWLockShared(&g_observer_lock);
    texture->Release();
    resource_interface->Release();
}

void observe_depth_target(
    ID3D11DeviceContext* context,
    ID3D11DepthStencilView* depth_target) {
    if (context == nullptr || depth_target == nullptr ||
        context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
        return;
    }
    if (!g_discovery_active.load(std::memory_order_acquire)) {
        observe_depth_activity(context, depth_target);
        return;
    }

    AcquireSRWLockShared(&g_observer_lock);
    if (!g_discovery_active.load(std::memory_order_relaxed)) {
        ReleaseSRWLockShared(&g_observer_lock);
        observe_depth_activity(context, depth_target);
        return;
    }
    ViewCacheEntry* cached = find_cached_view(depth_target);
    if (cached != nullptr &&
        cached->resource_index < kMaximumObservedResources) {
        ObservedDepthResource& resource =
            g_observed_resources[cached->resource_index];
        if (resource.identity != nullptr &&
            resource.generation == cached->resource_generation) {
            InterlockedIncrement64(&resource.bindings);
            ReleaseSRWLockShared(&g_observer_lock);
            finish_discovery_if_due();
            return;
        }
    }
    ReleaseSRWLockShared(&g_observer_lock);

    D3D11_DEPTH_STENCIL_VIEW_DESC view_description = {};
    depth_target->GetDesc(&view_description);

    ID3D11Resource* resource_interface = nullptr;
    ID3D11Texture2D* texture = nullptr;
    depth_target->GetResource(&resource_interface);
    if (resource_interface == nullptr || FAILED(resource_interface->QueryInterface(
            IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture))) ||
        texture == nullptr) {
        if (resource_interface != nullptr) {
            resource_interface->Release();
        }
        return;
    }

    D3D11_TEXTURE2D_DESC description = {};
    texture->GetDesc(&description);
    const UINT reference_width =
        g_backbuffer_width.load(std::memory_order_acquire);
    const UINT reference_height =
        g_backbuffer_height.load(std::memory_order_acquire);

    AcquireSRWLockExclusive(&g_observer_lock);
    if (g_discovery_active.load(std::memory_order_relaxed)) {
        UINT resource_index = find_resource(texture);
        bool new_resource = false;
        if (resource_index == kInvalidResourceIndex) {
            resource_index = select_resource_slot(
                description, reference_width, reference_height);
            new_resource = resource_index != kInvalidResourceIndex;
        }
        if (resource_index != kInvalidResourceIndex) {
            ObservedDepthResource& resource =
                g_observed_resources[resource_index];
            if (new_resource) {
                if (resource.generation == 0) {
                    resource.generation = 1;
                }
                resource.identity = texture;
                texture->AddRef();
                resource.texture = texture;
                resource.description = description;
                resource.width = description.Width;
                resource.height = description.Height;
                resource.texture_format = description.Format;
                resource.view_format = view_description.Format;
                resource.sample_count = description.SampleDesc.Count;
                resource.bind_flags = description.BindFlags;
                resource.bindings = 1;
                resource.observed_views = 1;
            } else {
                InterlockedIncrement64(&resource.bindings);
                ++resource.observed_views;
            }
            cache_view(
                depth_target, resource_index, resource.generation);
        }
    }
    ReleaseSRWLockExclusive(&g_observer_lock);

    texture->Release();
    resource_interface->Release();
    finish_discovery_if_due();
}

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

}  // namespace photorealism
