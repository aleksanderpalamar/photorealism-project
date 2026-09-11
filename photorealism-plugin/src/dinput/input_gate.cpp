#define DIRECTINPUT_VERSION 0x0800

#include "input_gate.hpp"

#include "../hooks/vtable_patch.hpp"
#include "menu_gate.hpp"

#include <dinput.h>

#include <atomic>
#include <cstring>

namespace photorealism {
namespace dinput {
namespace {

constexpr unsigned kCreateDeviceSlot = 3;
constexpr unsigned kGetDeviceStateSlot = 9;
constexpr unsigned kGetDeviceDataSlot = 10;
constexpr unsigned kMaximumGatedDevices = 8;

using CreateDeviceFunction = HRESULT(STDMETHODCALLTYPE*)(
    IDirectInput8W*, REFGUID, LPDIRECTINPUTDEVICE8W*, LPUNKNOWN);
using GetDeviceStateFunction =
    HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*, DWORD, LPVOID);
using GetDeviceDataFunction = HRESULT(STDMETHODCALLTYPE*)(
    IDirectInputDevice8W*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);

std::atomic<CreateDeviceFunction> g_original_create_device{nullptr};
std::atomic<GetDeviceStateFunction> g_original_get_device_state{nullptr};
std::atomic<GetDeviceDataFunction> g_original_get_device_data{nullptr};
std::atomic<void*> g_gated[kMaximumGatedDevices] = {};

void** vtable_entry(void* object, unsigned slot) {
    void** vtable = *reinterpret_cast<void***>(object);
    return &vtable[slot];
}

bool is_gated(void* device) {
    for (std::atomic<void*>& slot : g_gated) {
        if (slot.load(std::memory_order_acquire) == device) {
            return true;
        }
    }
    return false;
}

void remember(void* device) {
    for (std::atomic<void*>& slot : g_gated) {
        void* empty = nullptr;
        if (slot.compare_exchange_strong(empty, device)) {
            return;
        }
    }
}

bool wants_gating(IDirectInputDevice8W* device, DWORD* type_out) {
    DIDEVCAPS capabilities = {};
    capabilities.dwSize = sizeof(capabilities);
    if (FAILED(device->GetCapabilities(&capabilities))) {
        return false;
    }
    const DWORD type = GET_DIDEVICE_TYPE(capabilities.dwDevType);
    *type_out = type;
    return type == DI8DEVTYPE_MOUSE || type == DI8DEVTYPE_KEYBOARD;
}

HRESULT STDMETHODCALLTYPE hooked_get_device_state(
    IDirectInputDevice8W* device, DWORD size, LPVOID data) {
    const GetDeviceStateFunction original =
        g_original_get_device_state.load(std::memory_order_acquire);
    if (original == nullptr) {
        return DIERR_NOTINITIALIZED;
    }
    const HRESULT result = original(device, size, data);
    if (FAILED(result) || data == nullptr) {
        return result;
    }
    if (is_gated(device) && menu_is_capturing()) {
        std::memset(data, 0, size);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE hooked_get_device_data(
    IDirectInputDevice8W* device,
    DWORD size,
    LPDIDEVICEOBJECTDATA data,
    LPDWORD count,
    DWORD flags) {
    const GetDeviceDataFunction original =
        g_original_get_device_data.load(std::memory_order_acquire);
    if (original == nullptr) {
        return DIERR_NOTINITIALIZED;
    }
    const HRESULT result = original(device, size, data, count, flags);
    if (FAILED(result) || count == nullptr) {
        return result;
    }
    if (is_gated(device) && menu_is_capturing()) {
        *count = 0;
    }
    return result;
}

void patch_device(IDirectInputDevice8W* device) {
    replace_vtable_entry(
        vtable_entry(device, kGetDeviceStateSlot),
        reinterpret_cast<void*>(&hooked_get_device_state),
        &g_original_get_device_state);
    replace_vtable_entry(
        vtable_entry(device, kGetDeviceDataSlot),
        reinterpret_cast<void*>(&hooked_get_device_data),
        &g_original_get_device_data);
}

HRESULT STDMETHODCALLTYPE hooked_create_device(
    IDirectInput8W* factory,
    REFGUID device_guid,
    LPDIRECTINPUTDEVICE8W* device,
    LPUNKNOWN outer) {
    const CreateDeviceFunction original =
        g_original_create_device.load(std::memory_order_acquire);
    if (original == nullptr) {
        return DIERR_NOTINITIALIZED;
    }
    const HRESULT result = original(factory, device_guid, device, outer);
    if (FAILED(result) || device == nullptr || *device == nullptr) {
        return result;
    }

    DWORD type = 0;
    if (!wants_gating(*device, &type)) {
        gate_log("DirectInput criou um dispositivo tipo %lu, sem porteira.", type);
        return result;
    }
    remember(*device);
    patch_device(*device);
    gate_log(
        "DirectInput criou %s do jogo; o menu passa a silencia-lo quando aberto.",
        type == DI8DEVTYPE_MOUSE ? "o mouse" : "o teclado");
    return result;
}

}

void install_input_gate(void* factory) {
    if (factory == nullptr) {
        return;
    }
    const bool patched = replace_vtable_entry(
        vtable_entry(factory, kCreateDeviceSlot),
        reinterpret_cast<void*>(&hooked_create_device),
        &g_original_create_device);
    gate_log(
        "Porteira de DirectInput %s.",
        patched ? "instalada" : "nao pode ser instalada");
}

}
}
