#pragma once

#include "signatures.hpp"

namespace photorealism {

HRESULT STDMETHODCALLTYPE hooked_present(
    IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);
HRESULT STDMETHODCALLTYPE hooked_present1(
    IDXGISwapChain1* swap_chain,
    UINT sync_interval,
    UINT flags,
    const DXGI_PRESENT_PARAMETERS* parameters);
HRESULT STDMETHODCALLTYPE hooked_resize_buffers(
    IDXGISwapChain* swap_chain,
    UINT buffer_count,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT flags);

}
