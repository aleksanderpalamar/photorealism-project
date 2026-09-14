#include "frame_constants.hpp"

#include "../config/profile_state.hpp"
#include "shader_constants.hpp"

#include <cmath>

namespace photorealism {
namespace {

ProjectionScale projection_scale_for(
    const Settings& settings, const D3D11_TEXTURE2D_DESC& description) {
    constexpr float pi = 3.14159265358979323846f;
    const float vertical_fov_radians =
        settings.depth_vertical_fov * pi / 180.0f;
    const float y = 1.0f / std::tan(vertical_fov_radians * 0.5f);
    const float aspect =
        static_cast<float>(description.Width) /
        static_cast<float>(description.Height);
    return ProjectionScale{y / aspect, y};
}

void upload_visual_constants(
    ID3D11DeviceContext* context,
    const PipelineState& pipeline,
    const Settings& settings,
    const FrameConstantsInput& input) {
    ShaderConstants constants = {};
    constants.texel_size[0] =
        1.0f / static_cast<float>(input.description.Width);
    constants.texel_size[1] =
        1.0f / static_cast<float>(input.description.Height);
    constants.exposure = night_adjusted_exposure(settings, input.night_weight);
    constants.temperature = input.temperature;
    constants.contrast = settings.contrast;
    constants.saturation = settings.saturation;
    constants.vibrance = settings.vibrance;
    constants.shadows = settings.shadows;
    constants.highlights = settings.highlights;
    constants.blacks = settings.blacks;
    constants.whites = settings.whites;
    constants.local_contrast = settings.local_contrast;
    constants.sharpness = settings.sharpness;
    constants.vignette = settings.vignette;
    constants.black_lift[0] = settings.black_lift_r;
    constants.black_lift[1] = settings.black_lift_g;
    constants.black_lift[2] = settings.black_lift_b;
    constants.highlight_rolloff = settings.highlight_rolloff;
    constants.tint = input.tint;
    constants.bloom_enabled = input.bloom_active ? 1.0f : 0.0f;
    constants.bloom_intensity = settings.bloom_intensity;
    constants.input_needs_srgb_decode =
        reads_scene(*input.chain, EffectPass::Visual) &&
                input.scene_needs_srgb_decode
            ? 1.0f
            : 0.0f;
    constants.output_needs_srgb_encode =
        writes_output(*input.chain, EffectPass::Visual) &&
                input.output_needs_srgb_encode
            ? 1.0f
            : 0.0f;
    context->UpdateSubresource(
        pipeline.constants(ConstantSlot::Visual), 0, nullptr, &constants, 0, 0);
}

void upload_depth_constants(
    ID3D11DeviceContext* context,
    const PipelineState& pipeline,
    const Settings& settings,
    const FrameConstantsInput& input,
    const ProjectionScale& projection,
    float depth_texel_x,
    float depth_texel_y) {
    DepthPreviewConstants depth_constants = {};
    depth_constants.preview_mode =
        static_cast<float>(input.depth_preview_mode);
    depth_constants.output_needs_srgb_encode =
        input.output_needs_srgb_encode ? 1.0f : 0.0f;
    depth_constants.near_plane = settings.depth_near_plane;
    depth_constants.preview_distance = settings.depth_preview_distance;
    depth_constants.texel_size[0] = depth_texel_x;
    depth_constants.texel_size[1] = depth_texel_y;
    depth_constants.projection_scale[0] = projection.x;
    depth_constants.projection_scale[1] = projection.y;
    context->UpdateSubresource(
        pipeline.constants(ConstantSlot::DepthPreview), 0, nullptr,
        &depth_constants, 0, 0);
}

void upload_temporal_constants(
    ID3D11DeviceContext* context,
    const PipelineState& pipeline,
    const Settings& settings,
    const FrameConstantsInput& input) {
    TemporalConstants temporal_constants = {};
    temporal_constants.texel_size[0] =
        1.0f / static_cast<float>(input.description.Width);
    temporal_constants.texel_size[1] =
        1.0f / static_cast<float>(input.description.Height);
    temporal_constants.near_plane = settings.depth_near_plane;
    temporal_constants.history_weight =
        settings.temporal_history_weight;
    temporal_constants.depth_rejection =
        settings.temporal_depth_rejection;
    temporal_constants.color_rejection =
        settings.temporal_color_rejection;
    temporal_constants.history_valid =
        input.temporal_history_valid ? 1.0f : 0.0f;
    temporal_constants.output_needs_srgb_encode =
        writes_output(*input.chain, EffectPass::Temporal) &&
                input.output_needs_srgb_encode
            ? 1.0f
            : 0.0f;
    context->UpdateSubresource(
        pipeline.constants(ConstantSlot::Temporal),
        0,
        nullptr,
        &temporal_constants,
        0,
        0);
}

}  // namespace

void upload_frame_constants(
    ID3D11DeviceContext* context,
    const PipelineState& pipeline,
    const Settings& settings,
    const FrameConstantsInput& input) {
    const D3D11_TEXTURE2D_DESC& depth_source =
        input.depth_available ? input.depth_description : input.description;
    const float depth_texel_x =
        1.0f / static_cast<float>(depth_source.Width);
    const float depth_texel_y =
        1.0f / static_cast<float>(depth_source.Height);
    const ProjectionScale projection =
        projection_scale_for(settings, input.description);

    upload_visual_constants(context, pipeline, settings, input);
    upload_depth_constants(
        context, pipeline, settings, input, projection, depth_texel_x,
        depth_texel_y);
    upload_effect_constants(context, pipeline, settings, input, projection);
    upload_temporal_constants(context, pipeline, settings, input);
}

}  // namespace photorealism
