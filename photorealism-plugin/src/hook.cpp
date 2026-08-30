#include "hook.hpp"

#include "postprocess.hpp"
#include "resource_observer.hpp"
#include "runtime.hpp"
#include "steam_screenshots.hpp"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>

namespace photorealism {
namespace {

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
using CreateDeviceAndSwapChainFunction = decltype(&D3D11CreateDeviceAndSwapChain);

std::atomic<PresentFunction> g_original_present{nullptr};
std::atomic<Present1Function> g_original_present1{nullptr};
std::atomic<bool> g_present1_required{false};
std::atomic<ResizeBuffersFunction> g_original_resize_buffers{nullptr};
std::atomic<OMSetRenderTargetsFunction> g_original_set_render_targets{nullptr};
std::atomic<OMSetRenderTargetsAndUavsFunction>
    g_original_set_render_targets_and_uavs{nullptr};
std::atomic<ClearDepthStencilViewFunction>
    g_original_clear_depth_stencil_view{nullptr};
std::atomic<void**> g_present_vtable_entry{nullptr};
std::atomic<void**> g_present1_vtable_entry{nullptr};
std::atomic<bool> g_present_runtime_audited{false};
std::atomic<bool> g_present1_runtime_audited{false};
thread_local unsigned g_present_dispatch_depth = 0;

const char* module_name_for_address(const void* address, char* output, size_t size) {
    if (output == nullptr || size == 0) {
        return "indisponivel";
    }
    output[0] = '\0';
    if (address == nullptr) {
        std::snprintf(output, size, "null");
        return output;
    }
    HMODULE owner = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address),
            &owner) ||
        owner == nullptr) {
        std::snprintf(output, size, "owner-desconhecido");
        return output;
    }
    wchar_t wide_path[MAX_PATH] = {};
    if (GetModuleFileNameW(owner, wide_path, MAX_PATH) == 0 ||
        WideCharToMultiByte(
            CP_UTF8,
            0,
            wide_path,
            -1,
            output,
            static_cast<int>(size),
            nullptr,
            nullptr) == 0) {
        std::snprintf(output, size, "module=%p", static_cast<void*>(owner));
    }
    return output;
}

void log_present_entry(
    const char* phase,
    const char* method,
    void** entry,
    const void* replacement,
    const void* downstream) {
    void* current = entry != nullptr ? *entry : nullptr;
    char current_owner[MAX_PATH * 3] = {};
    char downstream_owner[MAX_PATH * 3] = {};
    log_message(
        "Auditoria Present 0.11.0: phase=%s method=%s entry=%p "
        "current=%p current_owner=%s ours=%p downstream=%p "
        "downstream_owner=%s state=%s.",
        phase != nullptr ? phase : "unknown",
        method,
        static_cast<void*>(entry),
        current,
        module_name_for_address(current, current_owner, sizeof(current_owner)),
        replacement,
        downstream,
        module_name_for_address(
            downstream, downstream_owner, sizeof(downstream_owner)),
        current == replacement ? "nosso-hook-externo" : "substituido-ou-encadeado");
}

class PresentDispatchScope {
public:
    PresentDispatchScope() : process_(g_present_dispatch_depth++ == 0) {}
    ~PresentDispatchScope() { --g_present_dispatch_depth; }
    bool should_process() const { return process_; }

private:
    bool process_;
};

CreateDeviceAndSwapChainFunction resolve_d3d11_create() {
    static CreateDeviceAndSwapChainFunction function =
        []() -> CreateDeviceAndSwapChainFunction {
        HMODULE d3d11 = GetModuleHandleW(L"d3d11.dll");
        if (d3d11 == nullptr) {
            d3d11 = LoadLibraryW(L"d3d11.dll");
        }
        if (d3d11 == nullptr) {
            return nullptr;
        }
        return reinterpret_cast<CreateDeviceAndSwapChainFunction>(
            GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain"));
    }();
    return function;
}

HRESULT STDMETHODCALLTYPE hooked_present(
    IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {
    PresentDispatchScope dispatch;
    if (dispatch.should_process()) {
        process_frame(swap_chain);
        observe_postprocessed_frame(swap_chain);
    }
    if (!g_present_runtime_audited.exchange(true, std::memory_order_acq_rel)) {
        log_present_entry(
            "first-runtime-call",
            "Present",
            g_present_vtable_entry.load(std::memory_order_acquire),
            reinterpret_cast<void*>(&hooked_present),
            reinterpret_cast<void*>(
                g_original_present.load(std::memory_order_acquire)));
    }
    const PresentFunction original =
        g_original_present.load(std::memory_order_acquire);
    if (original == nullptr) {
        return E_FAIL;
    }
    return original(swap_chain, sync_interval, flags);
}

HRESULT STDMETHODCALLTYPE hooked_present1(
    IDXGISwapChain1* swap_chain,
    UINT sync_interval,
    UINT flags,
    const DXGI_PRESENT_PARAMETERS* parameters) {
    PresentDispatchScope dispatch;
    if (dispatch.should_process()) {
        process_frame(swap_chain);
        observe_postprocessed_frame(swap_chain);
    }
    if (!g_present1_runtime_audited.exchange(true, std::memory_order_acq_rel)) {
        log_present_entry(
            "first-runtime-call",
            "Present1",
            g_present1_vtable_entry.load(std::memory_order_acquire),
            reinterpret_cast<void*>(&hooked_present1),
            reinterpret_cast<void*>(
                g_original_present1.load(std::memory_order_acquire)));
    }
    const Present1Function original =
        g_original_present1.load(std::memory_order_acquire);
    if (original == nullptr) {
        return E_FAIL;
    }
    return original(swap_chain, sync_interval, flags, parameters);
}

HRESULT STDMETHODCALLTYPE hooked_resize_buffers(
    IDXGISwapChain* swap_chain,
    UINT buffer_count,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT flags) {
    prepare_for_resize(
        swap_chain, buffer_count, width, height, format, flags);
    prepare_steam_screenshot_resize();
    const ResizeBuffersFunction original =
        g_original_resize_buffers.load(std::memory_order_acquire);
    if (original == nullptr) {
        return E_FAIL;
    }
    const HRESULT result = original(
        swap_chain, buffer_count, width, height, format, flags);
    report_resize_result(swap_chain, result);
    return result;
}

void STDMETHODCALLTYPE hooked_set_render_targets(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    ID3D11DepthStencilView* depth_target) {
    if (!is_processing_frame()) {
        observe_depth_target(context, depth_target);
    }
    const OMSetRenderTargetsFunction original =
        g_original_set_render_targets.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context, render_target_count, render_targets, depth_target);
    }
}

void STDMETHODCALLTYPE hooked_set_render_targets_and_uavs(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    ID3D11DepthStencilView* depth_target,
    UINT uav_start_slot,
    UINT uav_count,
    ID3D11UnorderedAccessView* const* unordered_views,
    const UINT* initial_counts) {
    if (!is_processing_frame()) {
        observe_depth_target(context, depth_target);
    }
    const OMSetRenderTargetsAndUavsFunction original =
        g_original_set_render_targets_and_uavs.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(
            context,
            render_target_count,
            render_targets,
            depth_target,
            uav_start_slot,
            uav_count,
            unordered_views,
            initial_counts);
    }
}

void STDMETHODCALLTYPE hooked_clear_depth_stencil_view(
    ID3D11DeviceContext* context,
    ID3D11DepthStencilView* depth_target,
    UINT clear_flags,
    FLOAT depth,
    UINT8 stencil) {
    if (!is_processing_frame()) {
        observe_depth_activity(context, depth_target);
    }
    const ClearDepthStencilViewFunction original =
        g_original_clear_depth_stencil_view.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context, depth_target, clear_flags, depth, stencil);
    }
}









template <typename Function>
bool replace_vtable_entry(
    void** entry,
    void* replacement,
    std::atomic<Function>* original_storage) {
    if (entry == nullptr || replacement == nullptr) {
        return false;
    }
    if (original_storage->load(std::memory_order_acquire) != nullptr) {
        return true;
    }

    DWORD old_protection = 0;
    if (!VirtualProtect(
            entry, sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protection)) {
        return false;
    }

    const Function original = reinterpret_cast<Function>(*entry);
    original_storage->store(original, std::memory_order_release);
    InterlockedExchangePointer(
        reinterpret_cast<void* volatile*>(entry), replacement);

    DWORD restored_protection = 0;
    VirtualProtect(
        entry, sizeof(void*), old_protection, &restored_protection);
    FlushInstructionCache(GetCurrentProcess(), entry, sizeof(void*));
    return true;
}

LRESULT CALLBACK hidden_window_proc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace

bool install_swap_chain_hooks() {
    if (g_original_present.load(std::memory_order_acquire) != nullptr &&
        (!g_present1_required.load(std::memory_order_acquire) ||
         g_original_present1.load(std::memory_order_acquire) != nullptr) &&
        g_original_resize_buffers.load(std::memory_order_acquire) != nullptr &&
        g_original_set_render_targets.load(std::memory_order_acquire) != nullptr &&
        g_original_set_render_targets_and_uavs.load(
            std::memory_order_acquire) != nullptr &&
        g_original_clear_depth_stencil_view.load(
            std::memory_order_acquire) != nullptr) {
        return true;
    }

    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* class_name = L"PhotorealismPluginD3D11Probe";
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = hidden_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name;
    RegisterClassExW(&window_class);

    HWND window = CreateWindowExW(
        0,
        class_name,
        L"Photorealism D3D11 probe",
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        64,
        64,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (window == nullptr) {
        log_message("Falha ao criar janela de prova D3D11: %lu.", GetLastError());
        return false;
    }

    DXGI_SWAP_CHAIN_DESC swap_description = {};
    swap_description.BufferDesc.Width = 64;
    swap_description.BufferDesc.Height = 64;
    swap_description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_description.SampleDesc.Count = 1;
    swap_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_description.BufferCount = 1;
    swap_description.OutputWindow = window;
    swap_description.Windowed = TRUE;
    swap_description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL requested_levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL selected_level = D3D_FEATURE_LEVEL_10_0;
    IDXGISwapChain* probe_swap_chain = nullptr;
    ID3D11Device* probe_device = nullptr;
    ID3D11DeviceContext* probe_context = nullptr;

    const CreateDeviceAndSwapChainFunction create_device =
        resolve_d3d11_create();
    if (create_device == nullptr) {
        log_message("D3D11CreateDeviceAndSwapChain nao foi localizado.");
        DestroyWindow(window);
        UnregisterClassW(class_name, instance);
        return false;
    }

    const HRESULT result = create_device(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        requested_levels,
        ARRAYSIZE(requested_levels),
        D3D11_SDK_VERSION,
        &swap_description,
        &probe_swap_chain,
        &probe_device,
        &selected_level,
        &probe_context);

    bool installed = false;
    if (SUCCEEDED(result) && probe_swap_chain != nullptr) {
        void** vtable = *reinterpret_cast<void***>(probe_swap_chain);
        void** present_entry = &vtable[8];
        g_present_vtable_entry.store(present_entry, std::memory_order_release);
        const bool present_installed = replace_vtable_entry(
            present_entry,
            reinterpret_cast<void*>(&hooked_present),
            &g_original_present);
        const bool resize_installed = replace_vtable_entry(
            &vtable[13],
            reinterpret_cast<void*>(&hooked_resize_buffers),
            &g_original_resize_buffers);
        bool present1_available = false;
        bool present1_installed = false;
        IDXGISwapChain1* probe_swap_chain1 = nullptr;
        if (SUCCEEDED(probe_swap_chain->QueryInterface(
                IID_IDXGISwapChain1,
                reinterpret_cast<void**>(&probe_swap_chain1))) &&
            probe_swap_chain1 != nullptr) {
            present1_available = true;
            g_present1_required.store(true, std::memory_order_release);
            void** vtable1 = *reinterpret_cast<void***>(probe_swap_chain1);
            void** present1_entry = &vtable1[22];
            g_present1_vtable_entry.store(
                present1_entry, std::memory_order_release);
            present1_installed = replace_vtable_entry(
                present1_entry,
                reinterpret_cast<void*>(&hooked_present1),
                &g_original_present1);
            probe_swap_chain1->Release();
        }
        bool depth_observer_installed = false;
        bool depth_uav_observer_installed = false;
        bool depth_clear_observer_installed = false;
        if (probe_context != nullptr) {
            void** context_vtable = *reinterpret_cast<void***>(probe_context);
            depth_observer_installed = replace_vtable_entry(
                &context_vtable[33],
                reinterpret_cast<void*>(&hooked_set_render_targets),
                &g_original_set_render_targets);
            depth_uav_observer_installed = replace_vtable_entry(
                &context_vtable[34],
                reinterpret_cast<void*>(&hooked_set_render_targets_and_uavs),
                &g_original_set_render_targets_and_uavs);
            depth_clear_observer_installed = replace_vtable_entry(
                &context_vtable[53],
                reinterpret_cast<void*>(&hooked_clear_depth_stencil_view),
                &g_original_clear_depth_stencil_view);
        }
        installed = present_installed &&
                    (!present1_available || present1_installed) &&
                    resize_installed &&
                    depth_observer_installed && depth_uav_observer_installed &&
                    depth_clear_observer_installed;
        if (installed) {
            // Os oito hooks de Draw/RSSet*/PSSetShaderResources sairam na
            // 0.15.0 junto com o modulo de upscaling que os exigia: existiam
            // so para alimenta-lo, e cada um custava uma indirecao em TODA
            // chamada de desenho do jogo. Restam os tres de que a descoberta
            // de depth depende. O CHANGELOG da 0.15.0 tem o nome do modulo; a
            // guarda de validate.sh proibe o acronimo aqui de proposito.
            log_message(
                "Hooks Present/Present1/ResizeBuffers/OMSetRenderTargets*/"
                "ClearDepthStencilView instalados; feature level=0x%X "
                "Present1=%s.",
                static_cast<unsigned>(selected_level),
                present1_available
                    ? (present1_installed ? "ativo-slot22" : "falha-slot22")
                    : "indisponivel");
            log_present_entry(
                "install",
                "Present",
                g_present_vtable_entry.load(std::memory_order_acquire),
                reinterpret_cast<void*>(&hooked_present),
                reinterpret_cast<void*>(
                    g_original_present.load(std::memory_order_acquire)));
            if (present1_available) {
                log_present_entry(
                    "install",
                    "Present1",
                    g_present1_vtable_entry.load(std::memory_order_acquire),
                    reinterpret_cast<void*>(&hooked_present1),
                    reinterpret_cast<void*>(
                        g_original_present1.load(std::memory_order_acquire)));
            }
        } else {
            log_message(
                "Falha parcial nos hooks: Present=%s Present1=%s "
                "ResizeBuffers=%s "
                "OMSetRenderTargets=%s OMSetRenderTargetsAndUAVs=%s "
                "ClearDepthStencilView=%s.",
                present_installed ? "ok" : "falha",
                present1_available
                    ? (present1_installed ? "ok" : "falha")
                    : "indisponivel",
                resize_installed ? "ok" : "falha",
                depth_observer_installed ? "ok" : "falha",
                depth_uav_observer_installed ? "ok" : "falha",
                depth_clear_observer_installed ? "ok" : "falha");
        }
    } else {
        log_message(
            "D3D11CreateDeviceAndSwapChain de prova falhou: 0x%08X.",
            static_cast<unsigned>(result));
    }

    if (probe_context != nullptr) {
        probe_context->Release();
    }
    if (probe_device != nullptr) {
        probe_device->Release();
    }
    if (probe_swap_chain != nullptr) {
        probe_swap_chain->Release();
    }
    DestroyWindow(window);
    UnregisterClassW(class_name, instance);
    return installed;
}

void audit_swap_chain_hook_chain(const char* phase) {
    void** present_entry =
        g_present_vtable_entry.load(std::memory_order_acquire);
    if (present_entry != nullptr) {
        log_present_entry(
            phase,
            "Present",
            present_entry,
            reinterpret_cast<void*>(&hooked_present),
            reinterpret_cast<void*>(
                g_original_present.load(std::memory_order_acquire)));
    }
    void** present1_entry =
        g_present1_vtable_entry.load(std::memory_order_acquire);
    if (present1_entry != nullptr) {
        log_present_entry(
            phase,
            "Present1",
            present1_entry,
            reinterpret_cast<void*>(&hooked_present1),
            reinterpret_cast<void*>(
                g_original_present1.load(std::memory_order_acquire)));
    }
}

}  // namespace photorealism
