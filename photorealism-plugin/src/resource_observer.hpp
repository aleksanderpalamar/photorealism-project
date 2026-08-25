#pragma once

#include <d3d11.h>
#include <dxgi.h>

#include <cstdint>

namespace photorealism {

void update_backbuffer_signature(
    UINT width, UINT height, DXGI_FORMAT format);
void restart_depth_discovery();
void restart_depth_discovery_for_device_change();
void observe_depth_target(
    ID3D11DeviceContext* context,
    ID3D11DepthStencilView* depth_target);
void observe_depth_activity(
    ID3D11DeviceContext* context,
    ID3D11DepthStencilView* depth_target);
bool acquire_depth_candidate(
    ID3D11Texture2D** texture,
    D3D11_TEXTURE2D_DESC* description,
    std::uint64_t* generation,
    std::uint64_t* binding_serial);
bool invalidate_stale_depth_candidate(
    std::uint64_t generation,
    std::uint64_t binding_serial);

}  // namespace photorealism
