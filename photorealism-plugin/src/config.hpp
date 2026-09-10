#pragma once

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

    bool scene_observer_enabled;
    float scene_observer_interval_frames;
    float scene_observer_log_seconds;

    bool condition_adaptation_enabled;
    float condition_time_constant_seconds;
    float condition_log_seconds;
    float condition_daylight_median_low;
    float condition_daylight_median_high;
    float condition_overcast_saturation_low;
    float condition_overcast_saturation_high;
    float condition_minimum_dynamic_range;
    float condition_sun_temperature;
    float condition_sun_tint;
    float condition_rain_temperature;
    float condition_rain_tint;
    float condition_night_temperature;
    float condition_night_tint;
};

Settings default_settings();
bool load_settings(Settings* settings);
}
