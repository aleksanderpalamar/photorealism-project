#pragma once

#include "../config/config.hpp"
#include "pipeline_state.hpp"

#include <d3d11.h>

namespace photorealism {

struct FrameConstantsInput {
    D3D11_TEXTURE2D_DESC description;
    D3D11_TEXTURE2D_DESC depth_description;
    unsigned depth_preview_mode;
    bool depth_available;
    bool ssao_active;
    bool ssao_preview;
    bool temporal_active;
    bool temporal_history_valid;
    bool bloom_active;
    bool scene_needs_srgb_decode;
    bool output_needs_srgb_encode;
    float temperature;
    float tint;
};

void upload_frame_constants(
    ID3D11DeviceContext* context,
    const PipelineState& pipeline,
    const Settings& settings,
    const FrameConstantsInput& input);

}  // namespace photorealism
