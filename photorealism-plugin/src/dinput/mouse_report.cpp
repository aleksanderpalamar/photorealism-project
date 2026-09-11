#include "mouse_report.hpp"

#include "menu_gate.hpp"

#include <atomic>
#include <cstring>

namespace photorealism {
namespace dinput {
namespace {

std::atomic<int> g_button_level{0};
std::atomic<int> g_axis_mode{0};

LONG read_axis(const BYTE* bytes, unsigned offset) {
    LONG value = 0;
    std::memcpy(&value, bytes + offset, sizeof(LONG));
    return value;
}

}

bool axes_are_relative(IDirectInputDevice8W* device) {
    const int known = g_axis_mode.load(std::memory_order_acquire);
    if (known != 0) {
        return known == 1;
    }

    DIPROPDWORD property = {};
    property.diph.dwSize = sizeof(DIPROPDWORD);
    property.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    property.diph.dwObj = 0;
    property.diph.dwHow = DIPH_DEVICE;
    if (FAILED(device->GetProperty(DIPROP_AXISMODE, &property.diph))) {
        g_axis_mode.store(1, std::memory_order_release);
        gate_log(
            "DirectInput nao informou o modo de eixo do mouse; o menu assume "
            "relativo.");
        return true;
    }

    const bool relative = property.dwData == DIPROPAXISMODE_REL;
    g_axis_mode.store(relative ? 1 : 2, std::memory_order_release);
    gate_log(
        "Mouse do DirectInput esta em modo %s.",
        relative ? "relativo; o menu move o ponteiro pelos deltas"
                 : "absoluto; o menu usa a posicao do cursor do sistema");
    return relative;
}

void report_state(const void* data, DWORD size) {
    if (size < sizeof(DIMOUSESTATE)) {
        return;
    }
    const BYTE* bytes = static_cast<const BYTE*>(data);
    const int buttons = (bytes[12] & 0x80) != 0 ? 1 : 0;
    g_button_level.store(buttons, std::memory_order_release);
    forward_mouse(
        1, read_axis(bytes, 0), read_axis(bytes, 4), read_axis(bytes, 8),
        buttons);
}

namespace {

void accumulate_entry(
    const DIDEVICEOBJECTDATA& entry, LONG* dx, LONG* dy, LONG* wheel) {
    if (entry.dwOfs == DIMOFS_X) {
        *dx += static_cast<LONG>(entry.dwData);
        return;
    }
    if (entry.dwOfs == DIMOFS_Y) {
        *dy += static_cast<LONG>(entry.dwData);
        return;
    }
    if (entry.dwOfs == DIMOFS_Z) {
        *wheel += static_cast<LONG>(entry.dwData);
        return;
    }
    if (entry.dwOfs != DIMOFS_BUTTON0) {
        return;
    }
    g_button_level.store(
        (entry.dwData & 0x80) != 0 ? 1 : 0, std::memory_order_release);
}

}

void report_data(const DIDEVICEOBJECTDATA* entries, DWORD count) {
    if (entries == nullptr) {
        return;
    }
    LONG dx = 0;
    LONG dy = 0;
    LONG wheel = 0;
    for (DWORD index = 0; index < count; ++index) {
        accumulate_entry(entries[index], &dx, &dy, &wheel);
    }
    forward_mouse(
        2, dx, dy, wheel, g_button_level.load(std::memory_order_acquire));
}

void clear_state(void* data, DWORD size) {
    std::memset(data, 0, size);
}

}
}
