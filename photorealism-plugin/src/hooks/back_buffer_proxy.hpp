#pragma once

#include <d3d11.h>
#include <dxgi.h>

namespace photorealism {

void patch_back_buffer_proxy(IDXGISwapChain* swap_chain);

bool present_back_buffer(
    IDXGISwapChain* swap_chain, ID3D11Texture2D** texture);

}
