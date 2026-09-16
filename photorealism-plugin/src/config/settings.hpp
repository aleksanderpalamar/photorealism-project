#pragma once

#include "photorealism_profile.hpp"

namespace photorealism {

struct Settings {
    bool enabled;
    float temperature;
    float exposure;
    float contrast;
    float saturation;
    float vibrance;
    float shadows;
    float highlights;
    float blacks;
    float whites;
    float local_contrast;
    float sharpness;
    float vignette;

    float black_lift_r;
    float black_lift_g;
    float black_lift_b;
    float highlight_rolloff;
    float tint;

    PhotorealismTonemap tonemap_sets[kProfileTonemapSets];
    float ssao_intensity_scale;
    float profile_night_exposure;
    float profile_pre_exposure;
    float profile_pre_contrast;
    float profile_dynamic_contrast;

    float profile_lighting_method;
    float profile_global_quality;
    float profile_taa;
    float profile_taa_level;
    float profile_dlss_preset;
    float profile_fxaa;
    float profile_sharpness;
    float profile_sharpen_edges;
    float profile_use_motion_blur;
    float profile_motion_blur_intensity;
    float profile_ssao_intensity;
    float profile_use_half_res_ssao;
    float profile_ssao_preset;
    float profile_ssao_detail_quality;
    float profile_lighting_interior;
    float profile_use_interior_lighting;
    float profile_use_default_mirrors;
    float profile_use_default_rain;
    float profile_use_sss;
    float profile_surface_albedo_saturation;
    float profile_roads_normal_intensity;
    float profile_roads_default_normals;
    float profile_vegetation_leaves_thickness;
    float profile_vegetation_grass_thickness;
    float profile_color_preset;
    float profile_color_preset_extra_brightness;
    float profile_tonemap_operator;
    float profile_tonemap_operator_a;
    float profile_hide_show_key;

    float depth_near_plane;
    float depth_preview_distance;
    float depth_vertical_fov;
    bool ssao_enabled;
    float ssao_radius;
    float ssao_intensity;
    float ssao_bias;
    float ssao_fade_start;
    float ssao_fade_end;
    float ssao_edge_rejection;
    bool ssao_refinement_enabled;
    float ssao_highlight_start;
    float ssao_highlight_end;
    float ssao_highlight_ao_floor;
    bool ssao_interior_enabled;
    float ssao_interior_near_start;
    float ssao_interior_near_end;
    float ssao_interior_radius;
    float ssao_interior_intensity;
    float ssao_interior_bias;
    float ssao_interior_edge_rejection;
    bool temporal_enabled;
    float temporal_history_weight;
    float temporal_depth_rejection;
    float temporal_color_rejection;
    bool bloom_enabled;
    float bloom_threshold;
    float bloom_knee;
    float bloom_intensity;
    float bloom_radius;

    bool fsr_enabled;
    float fsr_render_scale;
    float fsr_sharpness;
    float fsr_grain;

    bool wet_surface_enabled;
    float wet_roads_amount;
    float wet_roads_ripple;
    float wet_roads_gloss;
    float wet_roads_darkening;
    float wet_roads_floor;

    bool scene_observer_enabled;
    float scene_observer_interval_frames;
    float scene_observer_log_seconds;

    float condition_time_constant_seconds;
    float condition_log_seconds;
    float condition_daylight_median_low;
    float condition_daylight_median_high;
    float condition_overcast_saturation_low;
    float condition_overcast_saturation_high;
    float condition_minimum_dynamic_range;
};

}
