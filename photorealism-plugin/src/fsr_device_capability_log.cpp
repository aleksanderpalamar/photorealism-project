#include "fsr_device_capability_log.hpp"

#include "fsr_logging.hpp"

#include <dxgi.h>
#include <windows.h>

#include <cstdio>

namespace {

void log_adapter(ID3D11Device* device) {
    IDXGIDevice* dxgi_device = nullptr;
    HRESULT result = device->QueryInterface(
        IID_IDXGIDevice, reinterpret_cast<void**>(&dxgi_device));
    if (FAILED(result) || dxgi_device == nullptr) {
        log_message(
            "Adaptador DXGI indisponivel: result=0x%08X.",
            static_cast<unsigned>(result));
        return;
    }

    IDXGIAdapter* adapter = nullptr;
    result = dxgi_device->GetAdapter(&adapter);
    dxgi_device->Release();
    if (FAILED(result) || adapter == nullptr) {
        log_message(
            "GetAdapter falhou: result=0x%08X.",
            static_cast<unsigned>(result));
        return;
    }

    DXGI_ADAPTER_DESC description = {};
    result = adapter->GetDesc(&description);
    adapter->Release();
    if (FAILED(result)) {
        log_message(
            "GetDesc do adaptador falhou: result=0x%08X.",
            static_cast<unsigned>(result));
        return;
    }

    char adapter_name[256] = {};
    if (WideCharToMultiByte(
            CP_UTF8,
            0,
            description.Description,
            -1,
            adapter_name,
            static_cast<int>(sizeof(adapter_name)),
            nullptr,
            nullptr) == 0) {
        std::snprintf(adapter_name, sizeof(adapter_name), "indisponivel");
    }
    log_message(
        "Adapter: name=%s vendor=0x%04X device=0x%04X subsystem=0x%08X "
        "revision=0x%X dedicated_vram=%lluMiB shared_memory=%lluMiB "
        "luid=%08lX:%08lX.",
        adapter_name,
        description.VendorId,
        description.DeviceId,
        description.SubSysId,
        description.Revision,
        static_cast<unsigned long long>(
            description.DedicatedVideoMemory / (1024ull * 1024ull)),
        static_cast<unsigned long long>(
            description.SharedSystemMemory / (1024ull * 1024ull)),
        static_cast<unsigned long>(description.AdapterLuid.HighPart),
        static_cast<unsigned long>(description.AdapterLuid.LowPart));
}

void log_threading_support(ID3D11Device* device) {
    D3D11_FEATURE_DATA_THREADING threading = {};
    const HRESULT result = device->CheckFeatureSupport(
        D3D11_FEATURE_THREADING, &threading, sizeof(threading));
    if (FAILED(result)) {
        log_message(
            "Capacidades de threading indisponiveis: result=0x%08X.",
            static_cast<unsigned>(result));
        return;
    }
    log_message(
        "Capacidades D3D11: concurrent_creates=%s command_lists=%s.",
        threading.DriverConcurrentCreates ? "sim" : "nao",
        threading.DriverCommandLists ? "sim" : "nao");
}

void log_compute_support(ID3D11Device* device) {
    D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS compute_options = {};
    const HRESULT result = device->CheckFeatureSupport(
        D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS,
        &compute_options,
        sizeof(compute_options));
    log_message(
        "Compute shader 4.x raw/structured=%s (query=0x%08X); "
        "feature level 11.x ou superior habilita compute SM5 planejado.",
        SUCCEEDED(result) &&
                compute_options
                    .ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x
            ? "sim"
            : "nao",
        static_cast<unsigned>(result));
}

void log_format_support(
    ID3D11Device* device, DXGI_FORMAT format, const char* name) {
    UINT support = 0;
    const HRESULT result = device->CheckFormatSupport(format, &support);
    if (FAILED(result)) {
        log_message(
            "Formato %s: consulta falhou result=0x%08X.",
            name,
            static_cast<unsigned>(result));
        return;
    }
    log_message(
        "Formato %s: texture2d=%s sample=%s render_target=%s typed_uav=%s "
        "flags=0x%08X.",
        name,
        (support & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0 ? "sim" : "nao",
        (support & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0 ? "sim" : "nao",
        (support & D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0 ? "sim" : "nao",
        (support & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) != 0
            ? "sim"
            : "nao",
        support);
}

}  // namespace

void log_device_capabilities(ID3D11Device* device) {
    if (device == nullptr) {
        return;
    }
    log_adapter(device);
    log_threading_support(device);
    log_compute_support(device);
    log_format_support(
        device, DXGI_FORMAT_R16G16B16A16_FLOAT, "R16G16B16A16_FLOAT");
    log_format_support(device, DXGI_FORMAT_R11G11B10_FLOAT, "R11G11B10_FLOAT");
    log_format_support(device, DXGI_FORMAT_R8G8B8A8_UNORM, "R8G8B8A8_UNORM");
}
