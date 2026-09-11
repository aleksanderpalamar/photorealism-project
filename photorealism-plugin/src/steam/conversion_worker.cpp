#include "conversion_worker.hpp"

#include "capture_slots.hpp"

#include <windows.h>

namespace photorealism {
namespace steam {
namespace {

HANDLE g_worker_event = nullptr;
HANDLE g_worker_stop_event = nullptr;
HANDLE g_worker = nullptr;

void convert_slot_to_rgb(CaptureSlot* slot) {
    const bool bgra = capture_is_bgra();
    const std::size_t pixels =
        static_cast<std::size_t>(capture_width()) * capture_height();
    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        const std::uint8_t* source = &slot->raw[pixel * 4u];
        std::uint8_t* destination = &slot->rgb[pixel * 3u];
        destination[0] = source[bgra ? 2u : 0u];
        destination[1] = source[1];
        destination[2] = source[bgra ? 0u : 2u];
    }
}

bool convert_one_pending_slot() {
    BlockingCaptureLock lock;
    for (CaptureSlot& slot : capture_slots()) {
        const bool convertible = slot.state == CaptureState::cpu_pending &&
                                 slot.raw != nullptr && slot.rgb != nullptr;
        if (!convertible) {
            continue;
        }
        slot.state = CaptureState::converting;
        convert_slot_to_rgb(&slot);
        slot.state = CaptureState::ready;
        return true;
    }
    return false;
}

void close_worker_events() {
    if (g_worker_event != nullptr) {
        CloseHandle(g_worker_event);
        g_worker_event = nullptr;
    }
    if (g_worker_stop_event != nullptr) {
        CloseHandle(g_worker_stop_event);
        g_worker_stop_event = nullptr;
    }
}

DWORD WINAPI conversion_worker(LPVOID) {
    for (;;) {
        HANDLE events[2] = {g_worker_stop_event, g_worker_event};
        const DWORD wait = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) {
            return 0;
        }
        if (wait != WAIT_OBJECT_0 + 1) {
            return 1;
        }
        while (convert_one_pending_slot()) {
        }
    }
}

}

bool start_conversion_worker() {
    if (g_worker != nullptr) {
        return true;
    }
    g_worker_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_worker_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_worker_event == nullptr || g_worker_stop_event == nullptr) {
        close_worker_events();
        return false;
    }

    g_worker = CreateThread(nullptr, 0, conversion_worker, nullptr, 0, nullptr);
    if (g_worker != nullptr) {
        return true;
    }
    close_worker_events();
    return false;
}

void notify_conversion_worker() {
    if (g_worker_event == nullptr) {
        return;
    }
    SetEvent(g_worker_event);
}

void stop_conversion_worker() {
    HANDLE worker = g_worker;
    if (g_worker_stop_event != nullptr) {
        SetEvent(g_worker_stop_event);
    }
    notify_conversion_worker();
    if (worker != nullptr) {
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
    }
    g_worker = nullptr;
    close_worker_events();
}

}
}
