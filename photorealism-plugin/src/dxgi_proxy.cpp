#include "hook.hpp"
#include "runtime.hpp"

#include <dxgi1_3.h>
#include <windows.h>

#include <cwchar>

namespace {

HMODULE g_real_dxgi = nullptr;
INIT_ONCE g_dxgi_once = INIT_ONCE_STATIC_INIT;
INIT_ONCE g_core_once = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK load_real_dxgi(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t system_path[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(system_path, MAX_PATH);
    if (length == 0 || length + 10 >= MAX_PATH) {
        return FALSE;
    }
    std::wcscat(system_path, L"\\dxgi.dll");
    g_real_dxgi = LoadLibraryW(system_path);
    if (g_real_dxgi != nullptr) {
        photorealism::log_message(
            "Proxy DXGI conectado a implementacao do sistema.");
    } else {
        photorealism::log_message(
            "Falha ao carregar DXGI do sistema: %lu.", GetLastError());
    }
    return g_real_dxgi != nullptr;
}

FARPROC real_export(const char* name) {
    InitOnceExecuteOnce(&g_dxgi_once, load_real_dxgi, nullptr, nullptr);
    if (g_real_dxgi == nullptr) {
        return nullptr;
    }
    return GetProcAddress(g_real_dxgi, name);
}

DWORD WINAPI graphics_worker(LPVOID) {
    photorealism::log_message(
        "Photorealism Plugin 0.11.0: nucleo grafico carregado via dxgi.dll.");
    bool overlay_wait_completed = false;
    for (unsigned attempt = 0; attempt < 120; ++attempt) {
        if (GetModuleHandleW(L"d3d11.dll") == nullptr) {
            Sleep(500);
            continue;
        }
        if (!overlay_wait_completed) {
            constexpr unsigned kOverlayWaitSteps = 30;
            constexpr DWORD kOverlayWaitStepMilliseconds = 100;
            HMODULE overlay = nullptr;
            unsigned stable_samples = 0;
            for (unsigned step = 0; step < kOverlayWaitSteps; ++step) {
                HMODULE current =
                    GetModuleHandleW(L"gameoverlayrenderer64.dll");
                if (current != nullptr && current == overlay) {
                    ++stable_samples;
                } else {
                    overlay = current;
                    stable_samples = current != nullptr ? 1u : 0u;
                }
                if (overlay != nullptr && stable_samples >= 4) {
                    break;
                }
                Sleep(kOverlayWaitStepMilliseconds);
            }
            if (overlay != nullptr) {
                photorealism::log_message(
                    "Steam overlay detectado e estabilizado antes dos hooks: "
                    "gameoverlayrenderer64.dll=%p amostras=%u.",
                    static_cast<void*>(overlay),
                    stable_samples);
            } else {
                photorealism::log_message(
                    "Steam overlay nao detectado apos espera limitada de "
                    "3000ms; instalando hooks em modo fallback nao-Steam.");
            }
            overlay_wait_completed = true;
        }
        if (photorealism::install_swap_chain_hooks()) {
            Sleep(500);
            photorealism::audit_swap_chain_hook_chain("post-install-500ms");
            Sleep(1500);
            photorealism::audit_swap_chain_hook_chain("post-install-2000ms");
            Sleep(3000);
            photorealism::audit_swap_chain_hook_chain("post-install-5000ms");
            return 0;
        }
        Sleep(500);
    }
    photorealism::log_message(
        "Nao foi possivel instalar o hook D3D11 em 60 segundos.");
    return 1;
}

BOOL CALLBACK start_graphics_core_once(PINIT_ONCE, PVOID, PVOID*) {
    HANDLE worker = CreateThread(nullptr, 0, graphics_worker, nullptr, 0, nullptr);
    if (worker == nullptr) {
        photorealism::log_message(
            "Falha ao iniciar o nucleo grafico: %lu.", GetLastError());
        return FALSE;
    }
    CloseHandle(worker);
    return TRUE;
}

void ensure_graphics_core() {
    InitOnceExecuteOnce(&g_core_once, start_graphics_core_once, nullptr, nullptr);
}

}  // namespace

extern "C" HRESULT WINAPI CreateDXGIFactory(
    REFIID interface_id, void** output) {
    using Function = HRESULT(WINAPI*)(REFIID, void**);
    ensure_graphics_core();
    Function function = reinterpret_cast<Function>(real_export("CreateDXGIFactory"));
    return function != nullptr ? function(interface_id, output) : E_FAIL;
}

extern "C" HRESULT WINAPI CreateDXGIFactory1(
    REFIID interface_id, void** output) {
    using Function = HRESULT(WINAPI*)(REFIID, void**);
    ensure_graphics_core();
    Function function = reinterpret_cast<Function>(real_export("CreateDXGIFactory1"));
    return function != nullptr ? function(interface_id, output) : E_FAIL;
}

extern "C" HRESULT WINAPI CreateDXGIFactory2(
    UINT flags, REFIID interface_id, void** output) {
    using Function = HRESULT(WINAPI*)(UINT, REFIID, void**);
    ensure_graphics_core();
    Function function = reinterpret_cast<Function>(real_export("CreateDXGIFactory2"));
    return function != nullptr ? function(flags, interface_id, output) : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        photorealism::set_module(instance);
        DisableThreadLibraryCalls(instance);
        ensure_graphics_core();
    }
    return TRUE;
}
