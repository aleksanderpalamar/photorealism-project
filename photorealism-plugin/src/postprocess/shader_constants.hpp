#pragma once

#include <d3d11.h>

namespace photorealism {

struct ShaderConstants {
    float texel_size[2];
    float exposure;
    float temperature;
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
    float input_needs_srgb_decode;
    float output_needs_srgb_encode;

    float black_lift[3];
    float highlight_rolloff;
    float tint;
    float bloom_enabled;
    float bloom_intensity;
    float bloom_padding;
};

static_assert(sizeof(ShaderConstants) == 96, "constant buffer must be aligned");

struct BloomConstants {
    float source_texel_size[2];
    float filter_radius[2];
    float threshold;
    float knee;
    float input_needs_srgb_decode;
    float output_needs_srgb_encode;
};

static_assert(
    sizeof(BloomConstants) == 32,
    "bloom constant buffer must be aligned");

constexpr UINT kBloomPassCount = 3;

const char* const kBloomEntryPoints[kBloomPassCount] = {
    "PSBloomBright",
    "PSBloomDownsample",
    "PSBloomUpsample",
};

struct DepthPreviewConstants {
    float preview_mode;
    float output_needs_srgb_encode;
    float near_plane;
    float preview_distance;
    float texel_size[2];
    float projection_scale[2];
};

static_assert(
    sizeof(DepthPreviewConstants) == 32,
    "depth preview constant buffer must be aligned");

struct TemporalConstants {
    float texel_size[2];
    float near_plane;
    float history_weight;
    float depth_rejection;
    float color_rejection;
    float history_valid;
    float output_needs_srgb_encode;
};

static_assert(
    sizeof(TemporalConstants) == 32,
    "temporal constant buffer must be aligned");

}  // namespace photorealism
