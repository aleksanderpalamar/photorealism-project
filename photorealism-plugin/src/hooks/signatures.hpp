#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>

namespace photorealism {

using PresentFunction = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Present1Function = HRESULT(STDMETHODCALLTYPE*)(
    IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using ResizeBuffersFunction = HRESULT(STDMETHODCALLTYPE*)(
    IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using OMSetRenderTargetsFunction = void(STDMETHODCALLTYPE*)(
    ID3D11DeviceContext*,
    UINT,
    ID3D11RenderTargetView* const*,
    ID3D11DepthStencilView*);
using OMSetRenderTargetsAndUavsFunction = void(STDMETHODCALLTYPE*)(
    ID3D11DeviceContext*,
    UINT,
    ID3D11RenderTargetView* const*,
    ID3D11DepthStencilView*,
    UINT,
    UINT,
    ID3D11UnorderedAccessView* const*,
    const UINT*);
using ClearDepthStencilViewFunction = void(STDMETHODCALLTYPE*)(
    ID3D11DeviceContext*,
    ID3D11DepthStencilView*,
    UINT,
    FLOAT,
    UINT8);

}
