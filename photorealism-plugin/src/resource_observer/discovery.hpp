#pragma once

#include "types.hpp"

namespace photorealism {
namespace observer {

struct DiscoveryScan {
    ResourceSnapshot resources[kMaximumObservedResources];
    ResolutionGroup groups[kMaximumObservedResources];
    UINT resource_count;
    UINT group_count;
    UINT selected_index;
    UINT backbuffer_width;
    UINT backbuffer_height;
    UINT resource_evictions;
    UINT view_replacements;
    std::uint64_t completed_cycle;
    std::uint64_t selected_generation;
    ULONGLONG elapsed;
    bool candidate_retained;
    bool early_confidence;
};

bool run_discovery_pass(DiscoveryScan* scan);
void report_discovery(const DiscoveryScan& scan);
void finish_discovery_if_due();
void start_discovery(
    UINT width, UINT height, UINT format, const char* reason);

}
}
