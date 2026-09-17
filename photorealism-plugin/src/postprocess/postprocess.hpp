#pragma once

#include <d3d11.h>
#include <dxgi.h>

namespace photorealism {
void process_frame(IDXGISwapChain* swap_chain);
void upscale_present_frame(IDXGISwapChain* swap_chain);
void reconstruct_game_frame(
    ID3D11DeviceContext* context, ID3D11RenderTargetView* output);
void apply_pass_effects(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets);
void draw_overlay_frame(IDXGISwapChain* swap_chain);
bool is_processing_frame();
void prepare_for_resize(
    IDXGISwapChain* swap_chain,
    UINT buffer_count,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT flags);
void report_resize_result(IDXGISwapChain* swap_chain, HRESULT result);
}
