#pragma once

#include <cstdint>

namespace photorealism::fsr {

enum class ColorEvidenceLabel : std::uint32_t {
    unclassified,
    presentation,
    probable_scene,
    probable_reflection,
    probable_interface,
};

struct ColorEvidenceInput {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t backbuffer_width;
    std::uint32_t backbuffer_height;
    std::uint32_t views;
    std::uint64_t bindings;
    std::uint64_t slot_zero_bindings;
    std::uint64_t total_bindings;
    std::uint64_t first_serial;
    std::uint64_t last_serial;
    std::uint64_t total_serial;
    bool exact_backbuffer_resource;
};

struct ColorEvidenceResult {
    ColorEvidenceLabel label;
    std::uint32_t confidence;
    std::uint32_t scene_score;
    std::uint32_t reflection_score;
    std::uint32_t interface_score;
    std::uint32_t area_per_mille;
    std::uint32_t aspect_error_per_mille;
};

constexpr std::uint32_t ratio_per_mille(
    std::uint64_t numerator, std::uint64_t denominator) {
    return denominator == 0
               ? 0
               : static_cast<std::uint32_t>(
                     (numerator * 1000ull + denominator / 2ull) /
                     denominator);
}

constexpr ColorEvidenceResult classify_color_target(
    const ColorEvidenceInput& input) {
    ColorEvidenceResult result = {};
    result.label = ColorEvidenceLabel::unclassified;
    if (input.width == 0 || input.height == 0 ||
        input.backbuffer_width == 0 || input.backbuffer_height == 0) {
        return result;
    }

    const std::uint64_t area =
        static_cast<std::uint64_t>(input.width) * input.height;
    const std::uint64_t backbuffer_area =
        static_cast<std::uint64_t>(input.backbuffer_width) *
        input.backbuffer_height;
    result.area_per_mille = ratio_per_mille(area, backbuffer_area);

    const std::uint64_t aspect_left =
        static_cast<std::uint64_t>(input.width) * input.backbuffer_height;
    const std::uint64_t aspect_right =
        static_cast<std::uint64_t>(input.height) * input.backbuffer_width;
    const std::uint64_t aspect_difference =
        aspect_left > aspect_right ? aspect_left - aspect_right
                                   : aspect_right - aspect_left;
    result.aspect_error_per_mille =
        ratio_per_mille(aspect_difference, aspect_right);

    if (input.exact_backbuffer_resource) {
        result.label = ColorEvidenceLabel::presentation;
        result.confidence = 10;
        return result;
    }

    const std::uint32_t binding_share =
        ratio_per_mille(input.bindings, input.total_bindings);
    const std::uint32_t slot_zero_share =
        ratio_per_mille(input.slot_zero_bindings, input.bindings);
    const std::uint32_t first_position =
        ratio_per_mille(input.first_serial, input.total_serial);
    const std::uint32_t last_position =
        ratio_per_mille(input.last_serial, input.total_serial);
    const std::uint32_t activity_span =
        input.last_serial >= input.first_serial
            ? ratio_per_mille(
                  input.last_serial - input.first_serial, input.total_serial)
            : 0;

    if (result.area_per_mille >= 700 &&
        result.area_per_mille <= 2500) {
        result.scene_score += 3;
    }
    if (result.aspect_error_per_mille <= 100) {
        result.scene_score += 2;
    }
    if (binding_share >= 50) {
        result.scene_score += 2;
    }
    if (slot_zero_share >= 500) {
        result.scene_score += 1;
    }
    if (activity_span >= 400) {
        result.scene_score += 1;
    }
    if (first_position >= 700 && result.scene_score >= 2) {
        result.scene_score -= 2;
    }
    if (input.views > 64 && result.scene_score > 0) {
        result.scene_score -= 1;
    }

    if (result.area_per_mille >= 10 &&
        result.area_per_mille <= 650) {
        result.reflection_score += 3;
    }
    if (input.bindings >= 10) {
        result.reflection_score += 1;
    }
    if (binding_share >= 5) {
        result.reflection_score += 1;
    }
    if (first_position < 800) {
        result.reflection_score += 1;
    }
    if (input.views <= 32) {
        result.reflection_score += 1;
    }

    if (result.area_per_mille >= 700 &&
        result.area_per_mille <= 1400) {
        result.interface_score += 2;
    }
    if (result.aspect_error_per_mille <= 30) {
        result.interface_score += 2;
    }
    if (last_position >= 900) {
        result.interface_score += 2;
    }
    if (first_position >= 700) {
        result.interface_score += 2;
    }
    if (binding_share <= 150) {
        result.interface_score += 1;
    }

    if (result.interface_score >= 7 &&
        result.interface_score > result.scene_score) {
        result.label = ColorEvidenceLabel::probable_interface;
        result.confidence = result.interface_score;
    } else if (result.scene_score >= 7 &&
               result.scene_score >= result.reflection_score + 2) {
        result.label = ColorEvidenceLabel::probable_scene;
        result.confidence = result.scene_score;
    } else if (result.reflection_score >= 6 &&
               result.reflection_score > result.scene_score) {
        result.label = ColorEvidenceLabel::probable_reflection;
        result.confidence = result.reflection_score;
    }
    return result;
}

constexpr const char* color_evidence_label_name(ColorEvidenceLabel label) {
    switch (label) {
        case ColorEvidenceLabel::presentation:
            return "presentation-evidence";
        case ColorEvidenceLabel::probable_scene:
            return "probable-scene";
        case ColorEvidenceLabel::probable_reflection:
            return "probable-mirror-reflection";
        case ColorEvidenceLabel::probable_interface:
            return "probable-interface";
        default:
            return "unclassified";
    }
}

struct AutomaticSceneCandidateInput {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t format;
    std::uint32_t sample_count;
    std::uint32_t bind_flags;
    std::uint32_t misc_flags;
    std::uint32_t mip_levels;
    std::uint32_t array_size;
    std::uint32_t backbuffer_width;
    std::uint32_t backbuffer_height;
    std::uint32_t depth_width;
    std::uint32_t depth_height;
    std::uint64_t bindings;
    std::uint64_t slot_zero_bindings;
    std::uint64_t last_serial;
    std::uint64_t total_serial;
    std::uint64_t last_frame;
    std::uint64_t current_frame;
    std::uint64_t direct_composition_hits;
    std::uint64_t last_composition_frame;
    bool exact_backbuffer_resource;
};

struct AutomaticSceneCandidateResult {
    bool eligible;
    bool known_family;
    bool depth_correlated;
    bool native_composition;
    std::uint32_t confidence;
};

struct AutomaticCandidateSignature {
    std::uint32_t source_width;
    std::uint32_t source_height;
    std::uint32_t source_format;
    std::uint32_t output_width;
    std::uint32_t output_height;
};

constexpr std::uint64_t absolute_difference(
    std::uint64_t left, std::uint64_t right) {
    return left > right ? left - right : right - left;
}

constexpr bool is_fsr_color_format(std::uint32_t format) {
    return format == 10u || format == 26u || format == 28u ||
           format == 29u || format == 87u || format == 91u;
}

// Common DXGI formats: R16G16B16A16_FLOAT=10, R11G11B10_FLOAT=26,
// R8G8B8A8_UNORM(/SRGB)=28/29, B8G8R8A8_UNORM(/SRGB)=87/91.
// D3D11_BIND_SHADER_RESOURCE=0x8,
// D3D11_BIND_RENDER_TARGET=0x20 and D3D11_RESOURCE_MISC_TEXTURECUBE=0x4.
// Numeric values keep this pure classifier testable on the Linux build host.
constexpr AutomaticSceneCandidateResult score_automatic_scene_candidate_impl(
    const AutomaticSceneCandidateInput& input,
    bool require_draw_proof) {
    AutomaticSceneCandidateResult result = {};
    if (input.exact_backbuffer_resource || !is_fsr_color_format(input.format) ||
        input.sample_count != 1u || input.mip_levels != 1u ||
        input.array_size != 1u || (input.bind_flags & 0x28u) != 0x28u ||
        (input.misc_flags & 0x4u) != 0u || input.width == 0u ||
        input.height == 0u || input.current_frame < input.last_frame ||
        input.current_frame - input.last_frame > 2u ||
        (require_draw_proof &&
         (input.direct_composition_hits == 0u ||
          input.current_frame < input.last_composition_frame ||
          input.current_frame - input.last_composition_frame > 2u))) {
        return result;
    }

    // Runtime logs from ETS2/ATS 1.60 show the native post-tone-map scene
    // consumed directly at composition as R11G11B10_FLOAT.  Native candidates
    // use a deliberately stricter gate than scaled FSR candidates so another
    // full-size UI/intermediate texture cannot inherit temporal history.
    const bool native_or_supersampled_dimensions =
        input.width >= input.backbuffer_width &&
        input.height >= input.backbuffer_height;
    if (native_or_supersampled_dimensions) {
        if (input.format != 26u || input.slot_zero_bindings == 0u ||
            input.bindings < 12u ||
            (require_draw_proof && input.direct_composition_hits < 12u)) {
            return result;
        }
        const bool exact_native =
            input.width == input.backbuffer_width &&
            input.height == input.backbuffer_height;
        const bool observed_supersampled_family =
            (absolute_difference(input.width, 1920u) <= 16u &&
             absolute_difference(input.height, 1352u) <= 16u) ||
            (absolute_difference(input.width, 2400u) <= 16u &&
             absolute_difference(input.height, 1352u) <= 16u);
        result.native_composition = exact_native;
        result.known_family = observed_supersampled_family;
        result.depth_correlated =
            input.depth_width != 0u && input.depth_height != 0u &&
            absolute_difference(input.width, input.depth_width) <= 2u &&
            absolute_difference(input.height, input.depth_height) <= 2u;
        result.confidence = 170u;
        result.confidence += result.depth_correlated ? 25u : 0u;
        result.confidence += static_cast<std::uint32_t>(
            input.direct_composition_hits > 40u
                ? 40u
                : input.direct_composition_hits);
        result.eligible = result.native_composition || result.known_family ||
                          result.depth_correlated;
        return result;
    }
    if (input.width >= input.backbuffer_width ||
        input.height >= input.backbuffer_height) {
        return result;
    }

    const std::uint64_t aspect_left =
        static_cast<std::uint64_t>(input.width) * input.backbuffer_height;
    const std::uint64_t aspect_right =
        static_cast<std::uint64_t>(input.height) * input.backbuffer_width;
    if (ratio_per_mille(
            absolute_difference(aspect_left, aspect_right), aspect_right) >
        15u) {
        return result;
    }
    const std::uint32_t scale_x = ratio_per_mille(
        input.backbuffer_width, input.width);
    const std::uint32_t scale_y = ratio_per_mille(
        input.backbuffer_height, input.height);
    if (scale_x < 1050u || scale_y < 1050u || scale_x > 2000u ||
        scale_y > 2000u || absolute_difference(scale_x, scale_y) > 25u) {
        return result;
    }

    const bool ets_family =
        absolute_difference(input.width, 1920u) <= 16u &&
        absolute_difference(input.height, 1352u) <= 16u;
    const bool ats_family =
        absolute_difference(input.width, 2400u) <= 16u &&
        absolute_difference(input.height, 1352u) <= 16u;
    result.known_family = ets_family || ats_family;
    result.depth_correlated =
        input.depth_width != 0u && input.depth_height != 0u &&
        absolute_difference(input.width, input.depth_width) <= 2u &&
        absolute_difference(input.height, input.depth_height) <= 2u;
    result.confidence = 70u;
    result.confidence += result.known_family ? 35u : 0u;
    result.confidence += result.depth_correlated ? 30u : 0u;
    result.confidence +=
        input.format == 10u ? 8u : (input.format == 26u ? 20u : 28u);
    result.confidence += static_cast<std::uint32_t>(
        input.direct_composition_hits > 30u
            ? 30u
            : input.direct_composition_hits);
    result.confidence += input.last_frame == input.current_frame ? 15u : 8u;
    result.confidence += input.slot_zero_bindings != 0u ? 12u : 0u;
    result.confidence += static_cast<std::uint32_t>(
        input.bindings > 20u ? 20u : input.bindings);
    const std::uint32_t order =
        ratio_per_mille(input.last_serial, input.total_serial);
    if (order >= 300u && order <= 950u) {
        result.confidence += 8u;
    }
    result.eligible = result.confidence >= (require_draw_proof ? 130u : 110u);
    return result;
}

// Used only to locate a possible source before a final draw has been proven.
// It intentionally does not authorize processing or selection lock.
constexpr AutomaticSceneCandidateResult score_automatic_scene_candidate_base(
    const AutomaticSceneCandidateInput& input) {
    return score_automatic_scene_candidate_impl(input, false);
}

// Production selection requires evidence accumulated exclusively by a live
// pre-draw proof; PSSetShaderResources by itself is never sufficient.
constexpr AutomaticSceneCandidateResult score_automatic_scene_candidate(
    const AutomaticSceneCandidateInput& input) {
    return score_automatic_scene_candidate_impl(input, true);
}

constexpr bool automatic_candidate_is_better(
    const AutomaticSceneCandidateResult& candidate,
    std::uint64_t candidate_bindings,
    std::uint64_t candidate_last_serial,
    const AutomaticSceneCandidateResult& incumbent,
    std::uint64_t incumbent_bindings,
    std::uint64_t incumbent_last_serial) {
    if (candidate.eligible != incumbent.eligible) {
        return candidate.eligible;
    }
    if (candidate.confidence != incumbent.confidence) {
        return candidate.confidence > incumbent.confidence;
    }
    if (candidate_bindings != incumbent_bindings) {
        return candidate_bindings > incumbent_bindings;
    }
    return candidate_last_serial > incumbent_last_serial;
}

constexpr bool automatic_candidate_can_lock(
    const AutomaticSceneCandidateResult& candidate) {
    return candidate.eligible &&
           (candidate.native_composition || candidate.known_family ||
            candidate.depth_correlated);
}

struct FsrExecutionPolicy {
    bool execute;
    std::uint32_t scale_x_per_mille;
    std::uint32_t scale_y_per_mille;
    std::uint32_t aspect_error_per_mille;
};

constexpr FsrExecutionPolicy evaluate_fsr_execution_policy(
    std::uint32_t source_width,
    std::uint32_t source_height,
    std::uint32_t output_width,
    std::uint32_t output_height) {
    FsrExecutionPolicy policy = {};
    if (source_width == 0u || source_height == 0u || output_width == 0u ||
        output_height == 0u || source_width >= output_width ||
        source_height >= output_height) {
        return policy;
    }
    policy.scale_x_per_mille = ratio_per_mille(output_width, source_width);
    policy.scale_y_per_mille = ratio_per_mille(output_height, source_height);
    const std::uint64_t aspect_left =
        static_cast<std::uint64_t>(source_width) * output_height;
    const std::uint64_t aspect_right =
        static_cast<std::uint64_t>(source_height) * output_width;
    policy.aspect_error_per_mille = ratio_per_mille(
        absolute_difference(aspect_left, aspect_right), aspect_right);
    policy.execute = policy.scale_x_per_mille >= 1050u &&
                     policy.scale_y_per_mille >= 1050u &&
                     policy.scale_x_per_mille <= 2000u &&
                     policy.scale_y_per_mille <= 2000u &&
                     absolute_difference(
                         policy.scale_x_per_mille,
                         policy.scale_y_per_mille) <= 25u &&
                     policy.aspect_error_per_mille <= 15u;
    return policy;
}

constexpr bool automatic_candidate_signature_matches(
    const AutomaticCandidateSignature& left,
    const AutomaticCandidateSignature& right) {
    return left.source_width == right.source_width &&
           left.source_height == right.source_height &&
           left.source_format == right.source_format &&
           left.output_width == right.output_width &&
           left.output_height == right.output_height;
}

}  // namespace photorealism::fsr
