#include "defaults.hpp"

#include "../scene/condition_model.hpp"

namespace photorealism {
namespace {

CalibrationLayer reference_base() {
    CalibrationLayer layer = {};
    layer.enabled = true;
    layer.temperature = 6500.0f;

    layer.exposure = -0.0488697f;
    layer.contrast = 0.98f;
    layer.saturation = 0.95f;
    layer.vibrance = -0.05f;
    layer.shadows = 0.04f;
    layer.highlights = -0.05f;

    layer.blacks = 0.05f;
    layer.whites = 0.03f;
    layer.local_contrast = 0.12f;
    layer.sharpness = 0.18f;
    layer.vignette = 0.04f;

    layer.black_lift_r = 0.001017f;
    layer.black_lift_g = 0.001982f;
    layer.black_lift_b = 0.001888f;
    layer.highlight_rolloff = 0.35f;
    layer.tint = 0.35f;
    return layer;
}

CalibrationLayer visual_delta_0_2() {
    CalibrationLayer layer = {};
    layer.enabled = true;
    layer.temperature = -100.0f;
    layer.exposure = 0.05f;
    layer.contrast = 0.08f;
    layer.saturation = 0.03f;
    layer.vibrance = 0.09f;
    layer.shadows = 0.04f;
    layer.highlights = -0.09f;
    layer.blacks = -0.04f;
    layer.whites = 0.01f;
    layer.local_contrast = 0.06f;
    layer.sharpness = 0.04f;
    layer.vignette = -0.005f;

    layer.black_lift_r = 0.0f;
    layer.black_lift_g = 0.0f;
    layer.black_lift_b = 0.0f;
    layer.highlight_rolloff = 0.0f;
    layer.tint = 0.0f;
    return layer;
}

CalibrationLayer rain_overcast_delta_0_3() {
    CalibrationLayer layer = {};
    layer.enabled = true;
    layer.temperature = 0.0f;
    layer.exposure = 0.01f;
    layer.contrast = 0.01f;
    layer.saturation = -0.01f;
    layer.vibrance = 0.01f;
    layer.shadows = 0.02f;
    layer.highlights = -0.04f;
    layer.blacks = -0.01f;
    layer.whites = 0.04f;
    layer.local_contrast = 0.06f;
    layer.sharpness = -0.02f;
    layer.vignette = -0.005f;

    layer.black_lift_r = 0.000381f;
    layer.black_lift_g = 0.000498f;
    layer.black_lift_b = 0.000380f;
    layer.highlight_rolloff = 0.0f;

    layer.tint = 0.15f;
    return layer;
}

CalibrationLayer user_delta_0_20() {
    CalibrationLayer layer = {};
    layer.enabled = true;
    return layer;
}

}

CalibrationStack reference_stack() {
    const ConditionThresholds thresholds = default_condition_thresholds();
    const ConditionAnchors anchors = default_condition_anchors();

    CalibrationStack stack = {};
    stack.modules.enabled = true;
    stack.base = reference_base();
    stack.visual_0_2 = visual_delta_0_2();
    stack.rain_overcast_0_3 = rain_overcast_delta_0_3();
    stack.user_0_20 = user_delta_0_20();

    stack.modules.depth_near_plane = 0.1f;
    stack.modules.depth_preview_distance = 50.0f;
    stack.modules.depth_vertical_fov = 60.0f;

    stack.modules.ssao_enabled = true;
    stack.modules.ssao_radius = 0.8f;
    stack.modules.ssao_intensity = 0.28f;
    stack.modules.ssao_bias = 0.04f;
    stack.modules.ssao_fade_start = 30.0f;
    stack.modules.ssao_fade_end = 70.0f;
    stack.modules.ssao_edge_rejection = 1.5f;

    stack.modules.ssao_refinement_enabled = true;
    stack.modules.ssao_highlight_start = 0.55f;
    stack.modules.ssao_highlight_end = 0.95f;
    stack.modules.ssao_highlight_ao_floor = 0.35f;

    stack.modules.ssao_interior_enabled = true;
    stack.modules.ssao_interior_near_start = 2.0f;
    stack.modules.ssao_interior_near_end = 8.0f;
    stack.modules.ssao_interior_radius = 0.45f;
    stack.modules.ssao_interior_intensity = 0.20f;
    stack.modules.ssao_interior_bias = 0.05f;
    stack.modules.ssao_interior_edge_rejection = 1.75f;

    stack.modules.temporal_enabled = true;
    stack.modules.temporal_history_weight = 0.65f;
    stack.modules.temporal_depth_rejection = 0.02f;
    stack.modules.temporal_color_rejection = 0.08f;

    stack.modules.bloom_enabled = true;
    stack.modules.bloom_threshold = 0.85f;
    stack.modules.bloom_knee = 0.06f;
    stack.modules.bloom_intensity = 0.02f;
    stack.modules.bloom_radius = 0.03f;

    stack.modules.scene_observer_enabled = true;
    stack.modules.scene_observer_interval_frames = 30.0f;
    stack.modules.scene_observer_log_seconds = 30.0f;

    stack.modules.condition_adaptation_enabled = true;
    stack.modules.condition_time_constant_seconds = 180.0f;
    stack.modules.condition_log_seconds = 30.0f;
    stack.modules.condition_daylight_median_low = thresholds.daylight_median_low;
    stack.modules.condition_daylight_median_high = thresholds.daylight_median_high;
    stack.modules.condition_overcast_saturation_low =
        thresholds.overcast_saturation_low;
    stack.modules.condition_overcast_saturation_high =
        thresholds.overcast_saturation_high;
    stack.modules.condition_minimum_dynamic_range = thresholds.minimum_dynamic_range;
    stack.modules.condition_sun_temperature = anchors.sun_temperature;
    stack.modules.condition_sun_tint = anchors.sun_tint;
    stack.modules.condition_rain_temperature = anchors.rain_temperature;
    stack.modules.condition_rain_tint = anchors.rain_tint;
    stack.modules.condition_night_temperature = anchors.night_temperature;
    stack.modules.condition_night_tint = anchors.night_tint;
    return stack;
}
}
