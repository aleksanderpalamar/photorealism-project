#pragma once

#include "../config/config.hpp"

#include <d3d11.h>

#include <cstdint>

namespace photorealism {

struct FrameLogState {
    bool bloom_wait_logged;
    unsigned bloom_active_logged_levels;
    unsigned depth_preview_logged_mode;
    bool depth_preview_wait_logged;
    bool ssao_wait_logged;
    std::uint64_t ssao_active_logged_generation;
    bool temporal_wait_logged;
    std::uint64_t temporal_active_logged_generation;
};

struct FrameLogInput {
    D3D11_TEXTURE2D_DESC description;
    D3D11_TEXTURE2D_DESC depth_description;
    std::uint64_t depth_generation;
    unsigned depth_preview_mode;
    unsigned bloom_level_count;
    bool bloom_active;
    bool depth_preview;
    bool ssao_preview;
    bool ssao_active;
    bool temporal_active;
};

void log_frame_plan(
    const Settings& settings,
    const FrameLogInput& input,
    FrameLogState* state);

}  // namespace photorealism
