#include "device_state.hpp"

#include "com_utils.hpp"

namespace photorealism {

void capture_state(ID3D11DeviceContext* context, SavedState* state) {
    context->OMGetRenderTargets(
        D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
        state->render_targets,
        &state->depth_target);
    context->OMGetBlendState(
        &state->blend_state, state->blend_factor, &state->sample_mask);
    context->OMGetDepthStencilState(
        &state->depth_state, &state->stencil_reference);
    context->RSGetState(&state->rasterizer_state);
    context->RSGetViewports(&state->viewport_count, state->viewports);
    context->RSGetScissorRects(&state->scissor_count, state->scissors);
    context->IAGetInputLayout(&state->input_layout);
    context->IAGetPrimitiveTopology(&state->topology);
    context->VSGetShader(&state->vertex_shader, nullptr, nullptr);
    context->PSGetShader(&state->pixel_shader, nullptr, nullptr);
    context->GSGetShader(&state->geometry_shader, nullptr, nullptr);
    context->HSGetShader(&state->hull_shader, nullptr, nullptr);
    context->DSGetShader(&state->domain_shader, nullptr, nullptr);
    context->PSGetShaderResources(0, 4, state->pixel_resources);
    context->PSGetSamplers(0, 2, state->pixel_samplers);
    context->PSGetConstantBuffers(0, 1, &state->pixel_constant_buffer);
}

void restore_state(ID3D11DeviceContext* context, SavedState* state) {
    context->OMSetRenderTargets(
        D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
        state->render_targets,
        state->depth_target);
    context->OMSetBlendState(
        state->blend_state, state->blend_factor, state->sample_mask);
    context->OMSetDepthStencilState(
        state->depth_state, state->stencil_reference);
    context->RSSetState(state->rasterizer_state);
    context->RSSetViewports(state->viewport_count, state->viewports);
    context->RSSetScissorRects(state->scissor_count, state->scissors);
    context->IASetInputLayout(state->input_layout);
    context->IASetPrimitiveTopology(state->topology);
    context->VSSetShader(state->vertex_shader, nullptr, 0);
    context->PSSetShader(state->pixel_shader, nullptr, 0);
    context->GSSetShader(state->geometry_shader, nullptr, 0);
    context->HSSetShader(state->hull_shader, nullptr, 0);
    context->DSSetShader(state->domain_shader, nullptr, 0);
    context->PSSetShaderResources(0, 4, state->pixel_resources);
    context->PSSetSamplers(0, 2, state->pixel_samplers);
    context->PSSetConstantBuffers(0, 1, &state->pixel_constant_buffer);

    for (ID3D11RenderTargetView*& target : state->render_targets) {
        safe_release(target);
    }
    safe_release(state->depth_target);
    safe_release(state->blend_state);
    safe_release(state->depth_state);
    safe_release(state->rasterizer_state);
    safe_release(state->input_layout);
    safe_release(state->vertex_shader);
    safe_release(state->pixel_shader);
    safe_release(state->geometry_shader);
    safe_release(state->hull_shader);
    safe_release(state->domain_shader);
    for (ID3D11ShaderResourceView*& resource : state->pixel_resources) {
        safe_release(resource);
    }
    for (ID3D11SamplerState*& sampler : state->pixel_samplers) {
        safe_release(sampler);
    }
    safe_release(state->pixel_constant_buffer);
}

}  // namespace photorealism
