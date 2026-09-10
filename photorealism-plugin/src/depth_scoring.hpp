#pragma once

#include <cstdint>

namespace photorealism::depth_scoring {
constexpr std::uint64_t kMaximumBindingContribution = 100000;
constexpr std::uint64_t kMinimumCandidateBindings = 1000;
constexpr std::uint64_t kMinimumSceneBindingsPerSecond = 400;
constexpr std::uint64_t kMinimumScaledSceneAreaPercent = 110;

constexpr std::uint64_t kMinimumSceneAreaPercent = 95;

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

inline bool is_plausible_scene_shape(
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t reference_width,
    std::uint32_t reference_height) {
    if (width == 0 || height == 0 ||
        reference_width == 0 || reference_height == 0) {
        return false;
    }
    if (aspect_is_close(width, height, reference_width, reference_height)) {
        return true;
    }
    return width % reference_width == 0 && height % reference_height == 0;
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

    if (!is_plausible_scene_shape(
            width, height, reference_width, reference_height)) {
        return false;
    }

    const std::uint64_t area =
        static_cast<std::uint64_t>(width) * height;
    const std::uint64_t reference_area =
        static_cast<std::uint64_t>(reference_width) * reference_height;
    const bool native_scene =
        area * 100 >= reference_area * kMinimumSceneAreaPercent;
    const bool internally_scaled =
        area * 100 >= reference_area * kMinimumScaledSceneAreaPercent;
    const bool sustained_scene_activity =
        bindings * 1000 >=
        observation_milliseconds * kMinimumSceneBindingsPerSecond;
    return native_scene || internally_scaled || sustained_scene_activity;
}

enum class DepthRejection {
    none = 0,
    samples = 1,
    bindings = 2,
    too_small = 3,
    shape = 4,
    area_and_activity = 5,
    invalid = 6,
};

constexpr const char* depth_rejection_name(DepthRejection rejection) {
    switch (rejection) {
        case DepthRejection::none:
            return "aceito";
        case DepthRejection::samples:
            return "multisample";
        case DepthRejection::bindings:
            return "bindings-insuficientes";
        case DepthRejection::too_small:
            return "menor-que-metade-da-tela";
        case DepthRejection::shape:
            return "forma-incompativel";
        case DepthRejection::area_and_activity:
            return "area-e-atividade-insuficientes";
        case DepthRejection::invalid:
            return "dimensoes-invalidas";
    }
    return "desconhecido";
}

inline DepthRejection depth_candidate_rejection(
    std::uint32_t width,
    std::uint32_t height,
    std::uint64_t bindings,
    std::uint32_t sample_count,
    std::uint32_t reference_width,
    std::uint32_t reference_height,
    std::uint64_t observation_milliseconds) {
    if (width == 0 || height == 0 || reference_width == 0 ||
        reference_height == 0 || observation_milliseconds == 0) {
        return DepthRejection::invalid;
    }
    if (sample_count != 1) {
        return DepthRejection::samples;
    }
    if (bindings < kMinimumCandidateBindings) {
        return DepthRejection::bindings;
    }
    const std::uint64_t area =
        static_cast<std::uint64_t>(width) * height;
    const std::uint64_t reference_area =
        static_cast<std::uint64_t>(reference_width) * reference_height;
    if (area * 2 < reference_area) {
        return DepthRejection::too_small;
    }
    if (!is_plausible_scene_shape(
            width, height, reference_width, reference_height)) {
        return DepthRejection::shape;
    }
    if (!is_scene_candidate(
            width,
            height,
            bindings,
            sample_count,
            reference_width,
            reference_height,
            observation_milliseconds)) {
        return DepthRejection::area_and_activity;
    }
    return DepthRejection::none;
}
}
