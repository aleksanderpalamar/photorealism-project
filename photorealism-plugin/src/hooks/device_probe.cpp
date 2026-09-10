#include "device_probe.hpp"

#include "../runtime.hpp"

#include <windows.h>

namespace photorealism {
namespace {

using CreateDeviceAndSwapChainFunction =
    decltype(&D3D11CreateDeviceAndSwapChain);

constexpr wchar_t kProbeClassName[] = L"PhotorealismPluginD3D11Probe";

LRESULT CALLBACK hidden_window_proc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcW(window, message, wparam, lparam);
}

CreateDeviceAndSwapChainFunction resolve_d3d11_create() {
    static CreateDeviceAndSwapChainFunction function =
        []() -> CreateDeviceAndSwapChainFunction {
        HMODULE module = GetModuleHandleW(L"d3d11.dll");
        if (module == nullptr) {
            module = LoadLibraryW(L"d3d11.dll");
        }
        if (module == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<CreateDeviceAndSwapChainFunction>(
            GetProcAddress(module, "D3D11CreateDeviceAndSwapChain"));
    }();
    return function;
}

}

DeviceProbe::~DeviceProbe() {
    if (context_ != nullptr) {
        context_->Release();
    }
    if (device_ != nullptr) {
        device_->Release();
    }
    if (swap_chain_ != nullptr) {
        swap_chain_->Release();
    }
    destroy_window();
}

bool DeviceProbe::create_window() {
    instance_ = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = hidden_window_proc;
    window_class.hInstance = instance_;
    window_class.lpszClassName = kProbeClassName;
    RegisterClassExW(&window_class);

    window_ = CreateWindowExW(
        0,
        kProbeClassName,
        L"Photorealism D3D11 probe",
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        64,
        64,
        nullptr,
        nullptr,
        instance_,
        nullptr);
    if (window_ != nullptr) {
        return true;
    }
    log_message("Falha ao criar janela de prova D3D11: %lu.", GetLastError());
    return false;
}

void DeviceProbe::destroy_window() {
    if (window_ != nullptr) {
        DestroyWindow(window_);
        window_ = nullptr;
    }
    if (instance_ != nullptr) {
        UnregisterClassW(kProbeClassName, instance_);
        instance_ = nullptr;
    }
}

bool DeviceProbe::create() {
    if (!create_window()) {
        return false;
    }

    const CreateDeviceAndSwapChainFunction create_device =
        resolve_d3d11_create();
    if (create_device == nullptr) {
        log_message("D3D11CreateDeviceAndSwapChain nao foi localizado.");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC swap_description = {};
    swap_description.BufferDesc.Width = 64;
    swap_description.BufferDesc.Height = 64;
    swap_description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_description.SampleDesc.Count = 1;
    swap_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_description.BufferCount = 1;
    swap_description.OutputWindow = window_;
    swap_description.Windowed = TRUE;
    swap_description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL requested_levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    const HRESULT result = create_device(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        requested_levels,
        ARRAYSIZE(requested_levels),
        D3D11_SDK_VERSION,
        &swap_description,
        &swap_chain_,
        &device_,
        &feature_level_,
        &context_);
    if (SUCCEEDED(result) && swap_chain_ != nullptr) {
        return true;
    }
    log_message(
        "D3D11CreateDeviceAndSwapChain de prova falhou: 0x%08X.",
        static_cast<unsigned>(result));
    return false;
}

}
