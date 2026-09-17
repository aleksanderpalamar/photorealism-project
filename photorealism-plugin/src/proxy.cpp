#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#include <windows.h>

#include <cwchar>

#include "dinput/input_gate.hpp"
#include "fsr/game_scale.hpp"
#include "native_aa/native_aa_config.hpp"
#include "native_quality/native_quality_config.hpp"

namespace {
HMODULE g_proxy_module = nullptr;
HMODULE g_real_dinput = nullptr;
HMODULE g_graphics_proxy = nullptr;
INIT_ONCE g_dinput_once = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK load_real_dinput(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t system_path[MAX_PATH] = {};
    const UINT length = GetSystemDirectoryW(system_path, MAX_PATH);
    if (length == 0 || length + 13 >= MAX_PATH) {
        return FALSE;
    }
    std::wcscat(system_path, L"\\dinput8.dll");
    g_real_dinput = LoadLibraryW(system_path);
    return g_real_dinput != nullptr;
}

FARPROC real_export(const char* name) {
    InitOnceExecuteOnce(&g_dinput_once, load_real_dinput, nullptr, nullptr);
    if (g_real_dinput == nullptr) {
        return nullptr;
    }
    return GetProcAddress(g_real_dinput, name);
}

DWORD WINAPI bootstrap_graphics_proxy(LPVOID) {
    configure_native_aa_for_photorealism(g_proxy_module);
    configure_native_quality_for_photorealism(g_proxy_module);
    photorealism::fsr::apply_render_scale_to_game(g_proxy_module);
    wchar_t sibling_path[MAX_PATH] = {};
    if (g_proxy_module == nullptr ||
        GetModuleFileNameW(g_proxy_module, sibling_path, MAX_PATH) == 0) {
        OutputDebugStringW(
            L"Photorealism Plugin: caminho do bootstrap indisponivel.\n");
        return 1;
    }

    wchar_t* separator = std::wcsrchr(sibling_path, L'\\');
    if (separator == nullptr) {
        return 1;
    }
    separator[1] = L'\0';
    if (std::wcslen(sibling_path) + 9 >= MAX_PATH) {
        return 1;
    }
    std::wcscat(sibling_path, L"dxgi.dll");

    g_graphics_proxy = LoadLibraryW(sibling_path);
    if (g_graphics_proxy == nullptr) {
        OutputDebugStringW(
            L"Photorealism Plugin: falha ao carregar dxgi.dll irma.\n");
        return 1;
    }
    return 0;
}
}

extern "C" HRESULT WINAPI DirectInput8Create(
    HINSTANCE instance,
    DWORD version,
    REFIID interface_id,
    LPVOID* output,
    LPUNKNOWN outer) {
    using Function = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    Function function = reinterpret_cast<Function>(real_export("DirectInput8Create"));
    if (function == nullptr) {
        return E_FAIL;
    }
    const HRESULT result =
        function(instance, version, interface_id, output, outer);
    if (SUCCEEDED(result) && output != nullptr) {
        photorealism::dinput::install_input_gate(*output);
    }
    return result;
}

extern "C" HRESULT WINAPI DllCanUnloadNow() {
    using Function = HRESULT(WINAPI*)();
    Function function = reinterpret_cast<Function>(real_export("DllCanUnloadNow"));
    return function != nullptr ? function() : S_FALSE;
}

extern "C" HRESULT WINAPI DllGetClassObject(
    REFCLSID class_id, REFIID interface_id, LPVOID* output) {
    using Function = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
    Function function = reinterpret_cast<Function>(real_export("DllGetClassObject"));
    return function != nullptr ? function(class_id, interface_id, output)
                               : CLASS_E_CLASSNOTAVAILABLE;
}

extern "C" HRESULT WINAPI DllRegisterServer() {
    using Function = HRESULT(WINAPI*)();
    Function function = reinterpret_cast<Function>(real_export("DllRegisterServer"));
    return function != nullptr ? function() : E_FAIL;
}

extern "C" HRESULT WINAPI DllUnregisterServer() {
    using Function = HRESULT(WINAPI*)();
    Function function = reinterpret_cast<Function>(real_export("DllUnregisterServer"));
    return function != nullptr ? function() : E_FAIL;
}

extern "C" LPCDIDATAFORMAT WINAPI GetdfDIJoystick() {
    using Function = LPCDIDATAFORMAT(WINAPI*)();
    Function function = reinterpret_cast<Function>(real_export("GetdfDIJoystick"));
    return function != nullptr ? function() : nullptr;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_proxy_module = instance;
        DisableThreadLibraryCalls(instance);
        HANDLE worker = CreateThread(
            nullptr, 0, bootstrap_graphics_proxy, nullptr, 0, nullptr);
        if (worker != nullptr) {
            CloseHandle(worker);
        }
    }
    return TRUE;
}
