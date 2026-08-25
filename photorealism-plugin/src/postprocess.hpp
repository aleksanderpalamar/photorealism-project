#pragma once

#include <dxgi.h>

namespace photorealism {

void process_frame(IDXGISwapChain* swap_chain);
bool is_processing_frame();
void prepare_for_resize(
    IDXGISwapChain* swap_chain,
    UINT buffer_count,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT flags);
void report_resize_result(IDXGISwapChain* swap_chain, HRESULT result);

}  // namespace photorealism
