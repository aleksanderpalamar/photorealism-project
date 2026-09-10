#include "hooks/hook.hpp"
#include "overlay_watch.hpp"
#include "runtime.hpp"

#include <dxgi1_3.h>
#include <windows.h>

#include <cwchar>
#include <utility>

namespace {

constexpr wchar_t kSystemDxgiRelativePath[] = L"\\dxgi.dll";
constexpr UINT kSystemDxgiRelativePathLength =
    sizeof(kSystemDxgiRelativePath) / sizeof(kSystemDxgiRelativePath[0]);

constexpr wchar_t kDirect3DModule[] = L"d3d11.dll";
constexpr wchar_t kSteamOverlayModule[] = L"gameoverlayrenderer64.dll";

constexpr unsigned kInstallAttempts = 120;
constexpr DWORD kInstallRetryMilliseconds = 500;

constexpr unsigned kOverlayWaitSteps = 30;
constexpr DWORD kOverlayWaitStepMilliseconds = 100;
constexpr unsigned kOverlayStableSamplesRequired = 4;

HMODULE g_real_dxgi = nullptr;
INIT_ONCE g_dxgi_once = INIT_ONCE_STATIC_INIT;
INIT_ONCE g_core_once = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK load_real_dxgi(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t system_path[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(system_path, MAX_PATH);
    if (length == 0 || length + kSystemDxgiRelativePathLength >= MAX_PATH) {
        return FALSE;
    }

    std::wcscat(system_path, kSystemDxgiRelativePath);
    g_real_dxgi = LoadLibraryW(system_path);
    if (g_real_dxgi == nullptr) {
        photorealism::log_message(
            "Falha ao carregar DXGI do sistema: %lu.", GetLastError());
        return FALSE;
    }

    photorealism::log_message(
        "Proxy DXGI conectado a implementacao do sistema.");
    return TRUE;
}

FARPROC real_export(const char* name) {
    InitOnceExecuteOnce(&g_dxgi_once, load_real_dxgi, nullptr, nullptr);
    if (g_real_dxgi == nullptr) {
        return nullptr;
    }
    return GetProcAddress(g_real_dxgi, name);
}

photorealism::OverlayWatch wait_for_stable_overlay() {
    photorealism::OverlayWatch watch = {};
    for (unsigned step = 0; step < kOverlayWaitSteps; ++step) {
        watch = photorealism::advance_overlay_watch(
            watch, GetModuleHandleW(kSteamOverlayModule));
        if (photorealism::overlay_is_stable(
                watch, kOverlayStableSamplesRequired)) {
            return watch;
        }
        Sleep(kOverlayWaitStepMilliseconds);
    }
    return watch;
}

void log_overlay_state(const photorealism::OverlayWatch& watch) {
    if (watch.module == nullptr) {
        photorealism::log_message(
            "Steam overlay nao detectado apos espera limitada de "
            "3000ms; instalando hooks em modo fallback nao-Steam.");
        return;
    }

    photorealism::log_message(
        "Steam overlay detectado e estabilizado antes dos hooks: "
        "gameoverlayrenderer64.dll=%p amostras=%u.",
        watch.module,
        watch.stable_samples);
}

struct PostInstallAudit {
    DWORD delay_since_previous_milliseconds;
    const char* phase;
};

constexpr PostInstallAudit kPostInstallAudits[] = {
    {500, "post-install-500ms"},
    {1500, "post-install-2000ms"},
    {3000, "post-install-5000ms"},
};

void run_post_install_audits() {
    for (const PostInstallAudit& audit : kPostInstallAudits) {
        Sleep(audit.delay_since_previous_milliseconds);
        photorealism::audit_swap_chain_hook_chain(audit.phase);
    }
}

DWORD WINAPI graphics_worker(LPVOID) {
    photorealism::log_message(
        "Photorealism Plugin 0.11.0: nucleo grafico carregado via dxgi.dll.");

    bool overlay_reported = false;
    for (unsigned attempt = 0; attempt < kInstallAttempts; ++attempt) {
        if (GetModuleHandleW(kDirect3DModule) == nullptr) {
            Sleep(kInstallRetryMilliseconds);
            continue;
        }
        if (!overlay_reported) {
            log_overlay_state(wait_for_stable_overlay());
            overlay_reported = true;
        }
        if (photorealism::install_swap_chain_hooks()) {
            run_post_install_audits();
            return 0;
        }
        Sleep(kInstallRetryMilliseconds);
    }

    photorealism::log_message(
        "Nao foi possivel instalar o hook D3D11 em 60 segundos.");
    return 1;
}

BOOL CALLBACK start_graphics_core_once(PINIT_ONCE, PVOID, PVOID*) {
    const HANDLE worker =
        CreateThread(nullptr, 0, graphics_worker, nullptr, 0, nullptr);
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

template <typename Function, typename... Arguments>
HRESULT forward_to_system_dxgi(const char* name, Arguments&&... arguments) {
    ensure_graphics_core();
    const auto function = reinterpret_cast<Function>(real_export(name));
    if (function == nullptr) {
        return E_FAIL;
    }
    return function(std::forward<Arguments>(arguments)...);
}

}  // namespace

extern "C" HRESULT WINAPI CreateDXGIFactory(
    REFIID interface_id, void** output) {
    return forward_to_system_dxgi<HRESULT(WINAPI*)(REFIID, void**)>(
        "CreateDXGIFactory", interface_id, output);
}

extern "C" HRESULT WINAPI CreateDXGIFactory1(
    REFIID interface_id, void** output) {
    return forward_to_system_dxgi<HRESULT(WINAPI*)(REFIID, void**)>(
        "CreateDXGIFactory1", interface_id, output);
}

extern "C" HRESULT WINAPI CreateDXGIFactory2(
    UINT flags, REFIID interface_id, void** output) {
    return forward_to_system_dxgi<HRESULT(WINAPI*)(UINT, REFIID, void**)>(
        "CreateDXGIFactory2", flags, interface_id, output);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    photorealism::set_module(instance);
    DisableThreadLibraryCalls(instance);
    ensure_graphics_core();
    return TRUE;
}
