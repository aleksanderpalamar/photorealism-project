#include "discovery.hpp"

#include "../depth_scoring.hpp"
#include "../runtime.hpp"
#include "format_names.hpp"
#include "observer_state.hpp"

namespace photorealism {
namespace observer {

void report_discovery(const DiscoveryScan& scan) {
        log_message(
        "Descoberta depth 0.10.1 concluida: cycle=%llu mode=%s "
        "resources=%u grupos=%u "
        "resource_evictions=%u view_cache_replacements=%u janela=%llums "
        "minimum_bindings=%llu minimum_rate=%llu/s scaled_area=%llu%%.",
        static_cast<unsigned long long>(scan.completed_cycle),
        scan.early_confidence ? "early-confidence" : "window-timeout",
        scan.resource_count,
        scan.group_count,
        scan.resource_evictions,
        scan.view_replacements,
        static_cast<unsigned long long>(kDiscoveryDurationMilliseconds),
        static_cast<unsigned long long>(
            depth_scoring::kMinimumCandidateBindings),
        static_cast<unsigned long long>(
            depth_scoring::kMinimumSceneBindingsPerSecond),
        static_cast<unsigned long long>(
            depth_scoring::kMinimumScaledSceneAreaPercent));

    if (scan.candidate_retained) {
        const ResourceSnapshot& selected =
            scan.resources[scan.selected_index];
        log_message(
            "Depth principal 0.10.1 selecionado automaticamente: "
            "resource=%p generation=%llu "
            "size=%ux%u texture_format=%u(%s) samples=%u "
            "bind_flags=0x%08X bindings=%llu score=%llu.",
            selected.identity,
            static_cast<unsigned long long>(scan.selected_generation),
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
            static_cast<unsigned long long>(scan.completed_cycle + 1));
    }

    bool reported_groups[kMaximumObservedResources] = {};
    const UINT group_report_count = scan.group_count < kMaximumReportedGroups
                                        ? scan.group_count
                                        : kMaximumReportedGroups;
    for (UINT rank = 0; rank < group_report_count; ++rank) {
        UINT best = kInvalidResourceIndex;
        for (UINT index = 0; index < scan.group_count; ++index) {
            if (reported_groups[index]) {
                continue;
            }
            if (best == kInvalidResourceIndex ||
                scan.groups[index].score > scan.groups[best].score) {
                best = index;
            }
        }
        if (best == kInvalidResourceIndex) {
            break;
        }
        reported_groups[best] = true;
        const ResolutionGroup& group = scan.groups[best];
        const double area_scale = scan.backbuffer_width == 0 || scan.backbuffer_height == 0
                                      ? 0.0
                                      : static_cast<double>(group.width) *
                                            static_cast<double>(group.height) /
                                            (static_cast<double>(scan.backbuffer_width) *
                                             static_cast<double>(scan.backbuffer_height));
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
                scan.backbuffer_width,
                scan.backbuffer_height),
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
        scan.resource_count < kMaximumReportedResources
            ? scan.resource_count
            : kMaximumReportedResources;
    for (UINT rank = 0; rank < resource_report_count; ++rank) {
        UINT best = kInvalidResourceIndex;
        for (UINT index = 0; index < scan.resource_count; ++index) {
            if (reported_resources[index]) {
                continue;
            }
            if (best == kInvalidResourceIndex ||
                scan.resources[index].score > scan.resources[best].score) {
                best = index;
            }
        }
        if (best == kInvalidResourceIndex) {
            break;
        }
        reported_resources[best] = true;
        const ResourceSnapshot& resource = scan.resources[best];
        log_message(
            "Depth recurso #%u: resource=%p size=%ux%u "
            "texture_format=%u(%s) view_format=%u(%s) samples=%u "
            "bind_flags=0x%08X shader_readable=%s views=%u bindings=%llu "
            "score=%llu elegibilidade=%s.",
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
            static_cast<unsigned long long>(resource.score),
            depth_scoring::depth_rejection_name(
                depth_scoring::depth_candidate_rejection(
                    resource.width,
                    resource.height,
                    resource.bindings,
                    resource.sample_count,
                    scan.backbuffer_width,
                    scan.backbuffer_height,
                    scan.elapsed)));
    }
}
}

}
