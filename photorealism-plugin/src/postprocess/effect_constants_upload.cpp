#include "frame_constants.hpp"

#include "../config/effect_quality.hpp"
#include "../fsr/rcas_constants.hpp"
#include "effect_constants.hpp"

namespace photorealism {
namespace {

float flag(bool value) {
    return value ? 1.0f : 0.0f;
}

bool encodes(const FrameConstantsInput& input, EffectPass pass) {
    return writes_output(*input.chain, pass) && input.output_needs_srgb_encode;
}

void upload(
    ID3D11DeviceContext* context, const PipelineState& pipeline,
    ConstantSlot slot, const void* data) {
    context->UpdateSubresource(pipeline.constants(slot), 0, nullptr, data, 0, 0);
}

void upload_fxaa(
    ID3D11DeviceContext* context, const PipelineState& pipeline,
    const FrameConstantsInput& input) {
    FxaaConstants constants = {};
    constants.texel_size[0] = 1.0f / static_cast<float>(input.description.Width);
    constants.texel_size[1] = 1.0f / static_cast<float>(input.description.Height);
    constants.input_needs_srgb_decode = flag(input.scene_needs_srgb_decode);
    constants.output_needs_srgb_encode = flag(encodes(input, EffectPass::Fxaa));
    upload(context, pipeline, ConstantSlot::Fxaa, &constants);
}

void upload_occlusion(
    ID3D11DeviceContext* context, const PipelineState& pipeline,
    const Settings& settings, const FrameConstantsInput& input,
    const ProjectionScale& projection) {
    const SsaoQuality quality = ssao_quality(settings);
    const float strength = ssao_strength(settings);
    const D3D11_TEXTURE2D_DESC& depth =
        input.depth_available ? input.depth_description : input.description;
    OcclusionConstants constants = {};
    constants.near_plane = settings.depth_near_plane;
    constants.radius = settings.ssao_radius * quality.radius_scale;
    constants.intensity = settings.ssao_intensity * strength;
    constants.bias = settings.ssao_bias * quality.bias_scale;
    constants.fade_start = settings.ssao_fade_start;
    constants.fade_end = settings.ssao_fade_end;
    constants.edge_rejection = settings.ssao_edge_rejection;
    constants.sample_count = static_cast<float>(quality.samples);
    constants.depth_texel_size[0] = 1.0f / static_cast<float>(depth.Width);
    constants.depth_texel_size[1] = 1.0f / static_cast<float>(depth.Height);
    constants.projection_scale[0] = projection.x;
    constants.projection_scale[1] = projection.y;
    constants.interior_near_start = settings.ssao_interior_near_start;
    constants.interior_near_end = settings.ssao_interior_near_end;
    constants.interior_radius = settings.ssao_interior_radius * quality.radius_scale;
    constants.interior_intensity = settings.ssao_interior_intensity * strength;
    constants.interior_bias = settings.ssao_interior_bias * quality.bias_scale;
    constants.interior_edge_rejection = settings.ssao_interior_edge_rejection;
    constants.curve = quality.curve;
    upload(context, pipeline, ConstantSlot::Occlusion, &constants);
}

void upload_compose(
    ID3D11DeviceContext* context, const PipelineState& pipeline,
    const Settings& settings, const FrameConstantsInput& input) {
    const bool preview = input.ssao_preview;
    ComposeConstants constants = {};
    constants.input_needs_srgb_decode = flag(preview && input.scene_needs_srgb_decode);
    constants.output_needs_srgb_encode = flag(
        (preview || writes_output(*input.chain, EffectPass::Occlusion)) &&
        input.output_needs_srgb_encode);
    constants.debug_mode = flag(preview);
    constants.half_resolution = flag(ssao_quality(settings).half_resolution);
    constants.visibility_texel_size[0] =
        1.0f / static_cast<float>(input.occlusion_width > 0 ? input.occlusion_width : 1);
    constants.visibility_texel_size[1] =
        1.0f / static_cast<float>(input.occlusion_height > 0 ? input.occlusion_height : 1);
    constants.near_plane = settings.depth_near_plane;
    constants.refinement_enabled = flag(settings.ssao_refinement_enabled);
    constants.highlight_start = settings.ssao_highlight_start;
    constants.highlight_end = settings.ssao_highlight_end;
    constants.highlight_ao_floor = settings.ssao_highlight_ao_floor;
    upload(context, pipeline, ConstantSlot::Compose, &constants);
}

void upload_interior_light(
    ID3D11DeviceContext* context, const PipelineState& pipeline,
    const Settings& settings, const FrameConstantsInput& input) {
    InteriorLightConstants constants = {};
    constants.output_needs_srgb_encode =
        flag(encodes(input, EffectPass::InteriorLight));
    constants.near_plane = settings.depth_near_plane;
    constants.strength = interior_light_strength(settings);
    constants.near_start = kInteriorLightNearStart;
    constants.near_end = kInteriorLightNearEnd;
    constants.exterior_luma = input.exterior_luma;
    constants.gain = kInteriorLightGain;
    upload(context, pipeline, ConstantSlot::InteriorLight, &constants);
}

void upload_sharpen(
    ID3D11DeviceContext* context, const PipelineState& pipeline,
    const FrameConstantsInput& input) {
    fsr::RcasConstants constants = {};
    constants.rcas_con = fsr::rcas_con_from_stops(
        fsr::rcas_stops_from_sharpness(kClaritySharpness));
    constants.decode_before_write = flag(!input.output_needs_srgb_encode);
    constants.output_size[0] = static_cast<float>(input.description.Width);
    constants.output_size[1] = static_cast<float>(input.description.Height);
    constants.grain_tile_size = static_cast<float>(fsr::kGrainTileSize);
    upload(context, pipeline, ConstantSlot::Sharpen, &constants);
}

}

void upload_effect_constants(
    ID3D11DeviceContext* context,
    const PipelineState& pipeline,
    const Settings& settings,
    const FrameConstantsInput& input,
    const ProjectionScale& projection) {
    upload_fxaa(context, pipeline, input);
    upload_occlusion(context, pipeline, settings, input, projection);
    upload_compose(context, pipeline, settings, input);
    upload_interior_light(context, pipeline, settings, input);
    upload_sharpen(context, pipeline, input);
}

}
