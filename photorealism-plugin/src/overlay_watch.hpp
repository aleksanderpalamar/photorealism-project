#pragma once

namespace photorealism {

struct OverlayWatch {
    const void* module;
    unsigned stable_samples;
};

inline OverlayWatch advance_overlay_watch(
    const OverlayWatch& previous, const void* current) {
    const bool unchanged = current != nullptr && current == previous.module;
    OverlayWatch watch = {};
    watch.module = current;
    watch.stable_samples = unchanged ? previous.stable_samples + 1u
                                     : (current != nullptr ? 1u : 0u);
    return watch;
}

inline bool overlay_is_stable(
    const OverlayWatch& watch, unsigned required_samples) {
    return watch.module != nullptr && watch.stable_samples >= required_samples;
}

}  // namespace photorealism
