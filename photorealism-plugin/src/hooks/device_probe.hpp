#pragma once

#include <d3d11.h>
#include <dxgi.h>

namespace photorealism {

class DeviceProbe {
  public:
    ~DeviceProbe();

    bool create();

    IDXGISwapChain* swap_chain() const { return swap_chain_; }
    ID3D11DeviceContext* context() const { return context_; }
    D3D_FEATURE_LEVEL feature_level() const { return feature_level_; }

  private:
    bool create_window();
    void destroy_window();

    HWND window_ = nullptr;
    HINSTANCE instance_ = nullptr;
    IDXGISwapChain* swap_chain_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    D3D_FEATURE_LEVEL feature_level_ = D3D_FEATURE_LEVEL_10_0;
};

}
