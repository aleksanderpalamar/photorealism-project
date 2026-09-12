#include "back_buffer_proxy.hpp"

#include "../fsr/fsr_telemetry.hpp"
#include "../fsr/upscaler.hpp"
#include "../postprocess/com_utils.hpp"
#include "../postprocess/postprocess.hpp"
#include "vtable_patch.hpp"

#include <atomic>
#include <dxgi1_2.h>

namespace photorealism {
namespace {

constexpr unsigned kGetBufferSlot = 9;
constexpr unsigned kGetDescSlot = 12;
constexpr unsigned kGetDesc1Slot = 18;

using GetBufferFunction =
    HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, REFIID, void**);
using GetDescFunction =
    HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, DXGI_SWAP_CHAIN_DESC*);
using GetDesc1Function =
    HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, DXGI_SWAP_CHAIN_DESC1*);

std::atomic<GetBufferFunction> g_original_get_buffer{nullptr};
std::atomic<GetDescFunction> g_original_get_desc{nullptr};
std::atomic<GetDesc1Function> g_original_get_desc1{nullptr};

ID3D11Texture2D* proxy_for(IDXGISwapChain* swap_chain) {
    const GetBufferFunction original =
        g_original_get_buffer.load(std::memory_order_acquire);
    if (original == nullptr || !fsr::upscaler().wants_proxy()) {
        return nullptr;
    }

    ID3D11Texture2D* real = nullptr;
    if (FAILED(original(
            swap_chain, 0, IID_ID3D11Texture2D,
            reinterpret_cast<void**>(&real))) ||
        real == nullptr) {
        return nullptr;
    }

    D3D11_TEXTURE2D_DESC description = {};
    real->GetDesc(&description);
    ID3D11Device* device = nullptr;
    real->GetDevice(&device);
    real->Release();
    if (device == nullptr) {
        return nullptr;
    }

    ID3D11Texture2D* proxy = fsr::upscaler().ensure_proxy(device, description);
    device->Release();
    return proxy;
}

HRESULT STDMETHODCALLTYPE hooked_get_buffer(
    IDXGISwapChain* swap_chain, UINT index, REFIID interface_id, void** surface) {
    const GetBufferFunction original =
        g_original_get_buffer.load(std::memory_order_acquire);
    if (original == nullptr) {
        return E_FAIL;
    }
    if (index != 0 || surface == nullptr) {
        return original(swap_chain, index, interface_id, surface);
    }
    if (!is_processing_frame()) {
        fsr::telemetry().record_game_acquire();
    }

    ID3D11Texture2D* proxy = proxy_for(swap_chain);
    if (proxy == nullptr) {
        return original(swap_chain, index, interface_id, surface);
    }
    const HRESULT handed = proxy->QueryInterface(interface_id, surface);
    if (FAILED(handed)) {
        return original(swap_chain, index, interface_id, surface);
    }
    fsr::telemetry().record_replacement();
    return handed;
}

HRESULT STDMETHODCALLTYPE hooked_get_desc(
    IDXGISwapChain* swap_chain, DXGI_SWAP_CHAIN_DESC* description) {
    const GetDescFunction original =
        g_original_get_desc.load(std::memory_order_acquire);
    if (original == nullptr) {
        return E_FAIL;
    }
    const HRESULT result = original(swap_chain, description);
    if (FAILED(result) || description == nullptr) {
        return result;
    }
    const fsr::RenderExtent& extent = fsr::upscaler().extent();
    if (!fsr::upscaler().wants_proxy() || extent.width == 0) {
        return result;
    }
    description->BufferDesc.Width = extent.width;
    description->BufferDesc.Height = extent.height;
    return result;
}

HRESULT STDMETHODCALLTYPE hooked_get_desc1(
    IDXGISwapChain1* swap_chain, DXGI_SWAP_CHAIN_DESC1* description) {
    const GetDesc1Function original =
        g_original_get_desc1.load(std::memory_order_acquire);
    if (original == nullptr) {
        return E_FAIL;
    }
    const HRESULT result = original(swap_chain, description);
    if (FAILED(result) || description == nullptr) {
        return result;
    }
    const fsr::RenderExtent& extent = fsr::upscaler().extent();
    if (!fsr::upscaler().wants_proxy() || extent.width == 0) {
        return result;
    }
    description->Width = extent.width;
    description->Height = extent.height;
    return result;
}

}

void patch_back_buffer_proxy(IDXGISwapChain* swap_chain) {
    if (swap_chain == nullptr) {
        return;
    }
    void** vtable = *reinterpret_cast<void***>(swap_chain);
    replace_vtable_entry(
        &vtable[kGetBufferSlot],
        reinterpret_cast<void*>(&hooked_get_buffer),
        &g_original_get_buffer);
    replace_vtable_entry(
        &vtable[kGetDescSlot],
        reinterpret_cast<void*>(&hooked_get_desc),
        &g_original_get_desc);

    IDXGISwapChain1* swap_chain1 = nullptr;
    if (FAILED(swap_chain->QueryInterface(
            IID_IDXGISwapChain1, reinterpret_cast<void**>(&swap_chain1))) ||
        swap_chain1 == nullptr) {
        return;
    }
    void** vtable1 = *reinterpret_cast<void***>(swap_chain1);
    replace_vtable_entry(
        &vtable1[kGetDesc1Slot],
        reinterpret_cast<void*>(&hooked_get_desc1),
        &g_original_get_desc1);
    swap_chain1->Release();
}

bool present_back_buffer(
    IDXGISwapChain* swap_chain, ID3D11Texture2D** texture) {
    if (swap_chain == nullptr || texture == nullptr) {
        return false;
    }
    const GetBufferFunction original =
        g_original_get_buffer.load(std::memory_order_acquire);
    if (original == nullptr) {
        return SUCCEEDED(swap_chain->GetBuffer(
                   0, IID_ID3D11Texture2D,
                   reinterpret_cast<void**>(texture))) &&
               *texture != nullptr;
    }
    return SUCCEEDED(original(
               swap_chain, 0, IID_ID3D11Texture2D,
               reinterpret_cast<void**>(texture))) &&
           *texture != nullptr;
}

}
