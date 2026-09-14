#pragma once

#include "bloom_pyramid.hpp"
#include "depth_capture.hpp"
#include "effect_chain.hpp"
#include "effect_shaders.hpp"
#include "frame_resources.hpp"
#include "occlusion_target.hpp"
#include "pipeline_state.hpp"
#include "shader_library.hpp"
#include "temporal_history.hpp"

#include <d3d11.h>

namespace photorealism {

struct FramePassPlan {
    bool depth_preview;
    bool ssao_preview;
    bool bloom_preview;
    bool temporal;
    bool bloom;
};

struct FramePassScene {
    ID3D11DeviceContext* context;
    const PipelineState* pipeline;
    const ShaderLibrary* shaders;
    const EffectShaders* effects;
    const FrameResources* resources;
    DepthCapture* depth;
    TemporalHistory* temporal;
    BloomPyramid* bloom;
    OcclusionTarget* occlusion;
    const EffectChain* chain;
    ID3D11RenderTargetView* output;
    ID3D11Texture2D* back_buffer;
    D3D11_TEXTURE2D_DESC description;
    D3D11_TEXTURE2D_DESC depth_description;
    std::uint64_t depth_generation;
    bool output_needs_srgb_encode;
};

struct PassIo {
    ID3D11ShaderResourceView* source;
    ID3D11ShaderResourceView* raw_source;
    ID3D11RenderTargetView* target;
    ID3D11Texture2D* target_texture;
};

void bind_common_pipeline_state(const FramePassScene& scene);
void compose_output(const FramePassScene& scene, const FramePassPlan& plan);

void draw_fxaa_pass(const FramePassScene& scene, const FramePassPlan& plan, const PassIo& io);
void draw_visual_pass(const FramePassScene& scene, const FramePassPlan& plan, const PassIo& io);
void draw_interior_light_pass(
    const FramePassScene& scene, const FramePassPlan& plan, const PassIo& io);
void draw_occlusion_pass(
    const FramePassScene& scene, const FramePassPlan& plan, const PassIo& io);
void draw_temporal_pass(
    const FramePassScene& scene, const FramePassPlan& plan, const PassIo& io);
void draw_sharpen_pass(
    const FramePassScene& scene, const FramePassPlan& plan, const PassIo& io);
void run_effect_chain(const FramePassScene& scene, const FramePassPlan& plan);

}
