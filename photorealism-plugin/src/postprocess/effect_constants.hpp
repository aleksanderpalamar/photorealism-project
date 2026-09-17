#pragma once

namespace photorealism {

struct FxaaConstants {
    float texel_size[2];
    float input_needs_srgb_decode;
    float output_needs_srgb_encode;
};

static_assert(sizeof(FxaaConstants) == 16, "FXAA constant buffer must be aligned");

struct OcclusionConstants {
    float near_plane;
    float radius;
    float intensity;
    float bias;
    float fade_start;
    float fade_end;
    float edge_rejection;
    float sample_count;
    float depth_texel_size[2];
    float projection_scale[2];
    float interior_near_start;
    float interior_near_end;
    float interior_radius;
    float interior_intensity;
    float interior_bias;
    float interior_edge_rejection;
    float curve;
    float padding;
};

static_assert(
    sizeof(OcclusionConstants) == 80, "occlusion constant buffer must be aligned");

struct ComposeConstants {
    float input_needs_srgb_decode;
    float output_needs_srgb_encode;
    float debug_mode;
    float half_resolution;
    float visibility_texel_size[2];
    float near_plane;
    float refinement_enabled;
    float highlight_start;
    float highlight_end;
    float highlight_ao_floor;
    float padding;
};

static_assert(
    sizeof(ComposeConstants) == 48, "compose constant buffer must be aligned");

struct InteriorLightConstants {
    float input_needs_srgb_decode;
    float output_needs_srgb_encode;
    float near_plane;
    float strength;
    float near_start;
    float near_end;
    float exterior_luma;
    float gain;
};

static_assert(
    sizeof(InteriorLightConstants) == 32,
    "interior light constant buffer must be aligned");

}
