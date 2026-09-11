#include "raw_input.hpp"

#include "../runtime.hpp"

namespace photorealism {
namespace overlay {
namespace {

constexpr USHORT kGenericDesktop = 0x01;
constexpr USHORT kMouseUsage = 0x02;
constexpr USHORT kKeyboardUsage = 0x06;

bool is_pointer_or_keys(const RAWINPUTDEVICE& device) {
    return device.usUsagePage == kGenericDesktop &&
           (device.usUsage == kMouseUsage || device.usUsage == kKeyboardUsage);
}

bool read_registered(std::vector<RAWINPUTDEVICE>* devices) {
    UINT count = 0;
    GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE));
    if (count == 0) {
        return false;
    }
    devices->resize(count);
    const UINT written = GetRegisteredRawInputDevices(
        devices->data(), &count, sizeof(RAWINPUTDEVICE));
    if (written == static_cast<UINT>(-1)) {
        devices->clear();
        return false;
    }
    devices->resize(written);
    return true;
}

}

RawInputBlock& raw_input_block() {
    static RawInputBlock instance;
    return instance;
}

void RawInputBlock::suspend() {
    if (suspended_) {
        return;
    }
    suspended_ = true;
    saved_.clear();

    std::vector<RAWINPUTDEVICE> registered;
    if (!read_registered(&registered)) {
        if (!reported_) {
            log_message(
                "Menu: o jogo nao registrou entrada bruta; o mouse nao vem "
                "por esse caminho.");
            reported_ = true;
        }
        return;
    }

    std::vector<RAWINPUTDEVICE> removal;
    for (const RAWINPUTDEVICE& device : registered) {
        if (!is_pointer_or_keys(device)) {
            continue;
        }
        saved_.push_back(device);
        RAWINPUTDEVICE remove = {};
        remove.usUsagePage = device.usUsagePage;
        remove.usUsage = device.usUsage;
        remove.dwFlags = RIDEV_REMOVE;
        remove.hwndTarget = nullptr;
        removal.push_back(remove);
    }

    if (removal.empty()) {
        if (!reported_) {
            log_message(
                "Menu: o jogo registrou %u dispositivo(s) de entrada bruta, "
                "nenhum de mouse ou teclado.",
                static_cast<unsigned>(registered.size()));
            reported_ = true;
        }
        return;
    }

    const BOOL removed = RegisterRawInputDevices(
        removal.data(),
        static_cast<UINT>(removal.size()),
        sizeof(RAWINPUTDEVICE));
    log_message(
        "Menu suspendeu %u registro(s) de entrada bruta do jogo: %s.",
        static_cast<unsigned>(removal.size()),
        removed != FALSE ? "ok" : "falhou");
}

void RawInputBlock::restore() {
    if (!suspended_) {
        return;
    }
    suspended_ = false;
    if (saved_.empty()) {
        return;
    }

    const BOOL restored = RegisterRawInputDevices(
        saved_.data(),
        static_cast<UINT>(saved_.size()),
        sizeof(RAWINPUTDEVICE));
    log_message(
        "Menu devolveu %u registro(s) de entrada bruta ao jogo: %s.",
        static_cast<unsigned>(saved_.size()),
        restored != FALSE ? "ok" : "falhou");
    saved_.clear();
}

}
}
