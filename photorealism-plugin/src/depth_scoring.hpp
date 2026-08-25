#pragma once

#include <cstdint>

namespace photorealism::depth_scoring {

constexpr std::uint64_t kMaximumBindingContribution = 100000;
constexpr std::uint64_t kMinimumCandidateBindings = 1000;
constexpr std::uint64_t kMinimumSceneBindingsPerSecond = 400;
constexpr std::uint64_t kMinimumScaledSceneAreaPercent = 110;

inline bool aspect_is_close(
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t reference_width,
    std::uint32_t reference_height) {
    if (width == 0 || height == 0 ||
        reference_width == 0 || reference_height == 0) {
        return false;
    }
    const std::uint64_t left =
        static_cast<std::uint64_t>(width) * reference_height;
    const std::uint64_t right =
        static_cast<std::uint64_t>(height) * reference_width;
    const std::uint64_t difference = left > right ? left - right : right - left;
    const std::uint64_t scale = left > right ? left : right;
    return difference * 100 <= scale * 3;
}

inline std::uint64_t static_resource_priority(
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t reference_width,
    std::uint32_t reference_height) {
    if (width == 0 || height == 0 ||
        reference_width == 0 || reference_height == 0) {
        return 0;
    }

    std::uint64_t priority =
        static_cast<std::uint64_t>(width) * height;
    if (width >= reference_width && height >= reference_height) {
        priority *= 4;
    }
    if (width == reference_width || height == reference_height) {
        priority *= 4;
    }
    if (aspect_is_close(
            width, height, reference_width, reference_height)) {
        priority *= 4;
    }
    if (width == height && reference_width != reference_height) {
        priority /= 16;
    }
    return priority == 0 ? 1 : priority;
}

inline std::uint64_t resource_score(
    std::uint32_t width,
    std::uint32_t height,
    std::uint64_t bindings,
    std::uint32_t reference_width,
    std::uint32_t reference_height) {
    const std::uint64_t binding_contribution =
        bindings > kMaximumBindingContribution
            ? kMaximumBindingContribution
            : bindings;
    return static_resource_priority(
               width, height, reference_width, reference_height) *
           (binding_contribution + 1);
}

inline bool is_confident_candidate(
    std::uint32_t width,
    std::uint32_t height,
    std::uint64_t bindings,
    std::uint32_t sample_count,
    std::uint32_t reference_width,
    std::uint32_t reference_height) {
    if (sample_count != 1 || bindings < kMinimumCandidateBindings ||
        width == 0 || height == 0 ||
        reference_width == 0 || reference_height == 0) {
        return false;
    }

    const std::uint64_t area =
        static_cast<std::uint64_t>(width) * height;
    const std::uint64_t reference_area =
        static_cast<std::uint64_t>(reference_width) * reference_height;
    return area * 2 >= reference_area;
}

inline bool is_scene_candidate(
    std::uint32_t width,
    std::uint32_t height,
    std::uint64_t bindings,
    std::uint32_t sample_count,
    std::uint32_t reference_width,
    std::uint32_t reference_height,
    std::uint64_t observation_milliseconds) {
    if (!is_confident_candidate(
            width,
            height,
            bindings,
            sample_count,
            reference_width,
            reference_height) ||
        observation_milliseconds == 0) {
        return false;
    }

    const std::uint64_t area =
        static_cast<std::uint64_t>(width) * height;
    const std::uint64_t reference_area =
        static_cast<std::uint64_t>(reference_width) * reference_height;
    const bool internally_scaled =
        area * 100 >= reference_area * kMinimumScaledSceneAreaPercent;
    const bool sustained_scene_activity =
        bindings * 1000 >=
        observation_milliseconds * kMinimumSceneBindingsPerSecond;
    return internally_scaled || sustained_scene_activity;
}

}  // namespace photorealism::depth_scoring
