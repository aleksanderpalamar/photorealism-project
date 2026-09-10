#pragma once

#include "bloom_pyramid.hpp"
#include "depth_capture.hpp"
#include "frame_resources.hpp"
#include "pipeline_state.hpp"
#include "shader_library.hpp"
#include "temporal_history.hpp"

#include <d3d11.h>

namespace photorealism {

struct FramePassPlan {
    bool depth_preview;
    bool ssao_preview;
    bool bloom_preview;
    bool ssao;
    bool temporal;
    bool bloom;
};

struct FramePassScene {
    ID3D11DeviceContext* context;
    const PipelineState* pipeline;
    const ShaderLibrary* shaders;
    const FrameResources* resources;
    DepthCapture* depth;
    TemporalHistory* temporal;
    BloomPyramid* bloom;
    ID3D11RenderTargetView* output;
    ID3D11Texture2D* back_buffer;
    D3D11_TEXTURE2D_DESC description;
    D3D11_TEXTURE2D_DESC depth_description;
    std::uint64_t depth_generation;
    bool output_needs_srgb_encode;
};

void bind_common_pipeline_state(const FramePassScene& scene);
void compose_output(const FramePassScene& scene, const FramePassPlan& plan);

}  // namespace photorealism
