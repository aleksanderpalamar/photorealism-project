#include "defaults.hpp"

#include "../scene/condition_model.hpp"
#include "profile_fields.hpp"

namespace photorealism {
namespace {

void reference_profile_keys(Settings* settings) {
    settings->profile_lighting_method = kReferenceLightingMethod;
    settings->profile_global_quality = 0.0f;
    settings->profile_taa = 4.0f;
    settings->profile_taa_level = 1.0f;
    settings->profile_dlss_preset = 2.0f;
    settings->profile_fxaa = 1.0f;
    settings->profile_sharpness = 6.0f;
    settings->profile_sharpen_edges = 4.0f;
    settings->profile_use_motion_blur = 1.0f;
    settings->profile_motion_blur_intensity = 10.0f;
    settings->profile_ssao_intensity = 1.5f;
    settings->profile_use_half_res_ssao = 0.0f;
    settings->profile_ssao_preset = 0.0f;
    settings->profile_ssao_detail_quality = 0.0f;
    settings->profile_lighting_interior = 0.17f;
    settings->profile_use_interior_lighting = 1.0f;
    settings->profile_use_default_mirrors = 0.0f;
    settings->profile_use_default_rain = 0.0f;
    settings->profile_use_sss = 1.0f;
    settings->profile_surface_albedo_saturation = 1.07f;
    settings->profile_roads_normal_intensity = 3.0f;
    settings->profile_roads_default_normals = 1.0f;
    settings->profile_vegetation_leaves_thickness = 0.0f;
    settings->profile_vegetation_grass_thickness = 1.0f;
    settings->profile_color_preset = 0.0f;
    settings->profile_color_preset_extra_brightness = 0.0f;
    settings->profile_tonemap_operator = 0.0f;
    settings->profile_tonemap_operator_a = 7.0f;
    settings->profile_hide_show_key = 520.0f;
}

void reference_ambient_occlusion(Settings* settings) {
    settings->ssao_enabled = true;
    settings->ssao_radius = 0.8f;
    settings->ssao_intensity = 0.28f;
    settings->ssao_bias = 0.04f;
    settings->ssao_fade_start = 30.0f;
    settings->ssao_fade_end = 70.0f;
    settings->ssao_edge_rejection = 1.5f;

    settings->ssao_refinement_enabled = true;
    settings->ssao_highlight_start = 0.55f;
    settings->ssao_highlight_end = 0.95f;
    settings->ssao_highlight_ao_floor = 0.35f;

    settings->ssao_interior_enabled = true;
    settings->ssao_interior_near_start = 0.4f;
    settings->ssao_interior_near_end = 1.0f;
    settings->ssao_interior_radius = 0.45f;
    settings->ssao_interior_intensity = 0.20f;
    settings->ssao_interior_bias = 0.05f;
    settings->ssao_interior_edge_rejection = 1.75f;
}

void reference_night_detector(Settings* settings) {
    const ConditionThresholds thresholds = default_condition_thresholds();
    settings->condition_time_constant_seconds = 180.0f;
    settings->condition_log_seconds = 30.0f;
    settings->condition_daylight_median_low = thresholds.daylight_median_low;
    settings->condition_daylight_median_high = thresholds.daylight_median_high;
    settings->condition_overcast_saturation_low = thresholds.overcast_saturation_low;
    settings->condition_overcast_saturation_high =
        thresholds.overcast_saturation_high;
    settings->condition_minimum_dynamic_range = thresholds.minimum_dynamic_range;
}

}

Settings reference_settings() {
    Settings settings = {};
    settings.enabled = true;
    reference_tonemap_sets(settings.tonemap_sets);
    reference_profile_keys(&settings);

    settings.depth_near_plane = 0.1f;
    settings.depth_preview_distance = 50.0f;
    settings.depth_vertical_fov = 60.0f;
    reference_ambient_occlusion(&settings);

    settings.temporal_history_weight = 0.65f;
    settings.temporal_depth_rejection = 0.02f;
    settings.temporal_color_rejection = 0.08f;

    settings.bloom_enabled = true;
    settings.bloom_threshold = 0.85f;
    settings.bloom_knee = 0.06f;
    settings.bloom_intensity = 0.02f;
    settings.bloom_radius = 0.03f;

    settings.fsr_enabled = false;
    settings.fsr_render_scale = 0.8660f;
    settings.fsr_sharpness = 0.60f;
    settings.fsr_grain = 0.30f;

    settings.shader_patch_enabled = true;
    settings.wet_surface_enabled = true;
    settings.wet_roads_amount = 0.0f;
    settings.wet_roads_ripple = 0.5f;
    settings.wet_roads_gloss = 0.7f;
    settings.wet_roads_darkening = 0.45f;
    settings.wet_roads_floor = 0.0f;

    settings.scene_observer_enabled = true;
    settings.scene_observer_interval_frames = 30.0f;
    settings.scene_observer_log_seconds = 30.0f;
    reference_night_detector(&settings);
    return settings;
}

void restore_profile_defaults(Settings* settings) {
    const Settings reference = reference_settings();
    for (unsigned index = 0; index < kProfileTonemapSets; ++index) {
        settings->tonemap_sets[index] = reference.tonemap_sets[index];
    }
    for (std::size_t index = 0; index < kProfileFieldCount; ++index) {
        const ModuleField& field = kProfileFields[index];
        settings->*(field.member) = reference.*(field.member);
    }
}

}
