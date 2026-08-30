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
    float black_lift;
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
};

Settings default_settings();
bool load_settings(Settings* settings);

}  // namespace photorealism
