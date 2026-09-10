#pragma once

#include "signatures.hpp"

namespace photorealism {

void STDMETHODCALLTYPE hooked_set_render_targets(
    ID3D11DeviceContext* context,
    UINT target_count,
    ID3D11RenderTargetView* const* targets,
    ID3D11DepthStencilView* depth_target);
void STDMETHODCALLTYPE hooked_set_render_targets_and_uavs(
    ID3D11DeviceContext* context,
    UINT target_count,
    ID3D11RenderTargetView* const* targets,
    ID3D11DepthStencilView* depth_target,
    UINT uav_start,
    UINT uav_count,
    ID3D11UnorderedAccessView* const* uavs,
    const UINT* uav_initial_counts);
void STDMETHODCALLTYPE hooked_clear_depth_stencil_view(
    ID3D11DeviceContext* context,
    ID3D11DepthStencilView* view,
    UINT clear_flags,
    FLOAT depth,
    UINT8 stencil);

}
