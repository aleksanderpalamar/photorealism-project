#pragma once

#include "../config/config.hpp"
#include "effect_chain.hpp"
#include "pipeline_state.hpp"

#include <d3d11.h>

namespace photorealism {

struct ProjectionScale {
    float x;
    float y;
};

struct FrameConstantsInput {
    D3D11_TEXTURE2D_DESC description;
    D3D11_TEXTURE2D_DESC depth_description;
    unsigned depth_preview_mode;
    bool depth_available;
    bool ssao_preview;
    bool temporal_history_valid;
    bool bloom_active;
    bool scene_needs_srgb_decode;
    bool output_needs_srgb_encode;
    float temperature;
    float tint;
    float night_weight;
    float exterior_luma;
    UINT occlusion_width;
    UINT occlusion_height;
    const EffectChain* chain;
};

void upload_frame_constants(
    ID3D11DeviceContext* context,
    const PipelineState& pipeline,
    const Settings& settings,
    const FrameConstantsInput& input);

void upload_effect_constants(
    ID3D11DeviceContext* context,
    const PipelineState& pipeline,
    const Settings& settings,
    const FrameConstantsInput& input,
    const ProjectionScale& projection);

}
