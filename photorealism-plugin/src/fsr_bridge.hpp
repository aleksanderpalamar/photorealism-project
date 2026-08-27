#pragma once

#include <d3d11.h>

#include <cstdint>

namespace photorealism {

// The auxiliary module is optional.  Failure to load or initialize it never
// changes the result of the established visual 0.10.1 rendering pipeline.
void initialize_fsr_module(ID3D11Device* device);
void shutdown_fsr_device();
void observe_fsr_color_targets(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    bool includes_uavs);
void report_fsr_frame(
    ID3D11Texture2D* back_buffer,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT sample_count);
void reset_fsr_color_observation_for_resize();
void set_fsr_processing_enabled(bool enabled);
bool fsr_processing_enabled();
void report_fsr_automatic_selection_context(
    UINT backbuffer_width,
    UINT backbuffer_height,
    DXGI_FORMAT backbuffer_format,
    UINT depth_width,
    UINT depth_height,
    std::uint64_t depth_generation);
void observe_fsr_pixel_shader_resources(
    ID3D11DeviceContext* context,
    UINT start_slot,
    UINT view_count,
    ID3D11ShaderResourceView* const* views);
void observe_fsr_final_draw(
    ID3D11DeviceContext* context,
    std::uint32_t kind,
    UINT primitive_count,
    UINT instance_count,
    UINT start_location,
    INT base_vertex_location,
    UINT start_instance_location);
void observe_fsr_rasterizer_state(
    ID3D11DeviceContext* context, ID3D11RasterizerState* state);
void observe_fsr_viewports(
    ID3D11DeviceContext* context,
    UINT viewport_count,
    const D3D11_VIEWPORT* viewports);
void observe_fsr_scissor_rects(
    ID3D11DeviceContext* context,
    UINT scissor_count,
    const D3D11_RECT* scissors);

}  // namespace photorealism
