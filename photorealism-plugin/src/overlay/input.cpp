#include "input.hpp"

#include "../runtime.hpp"
#include "pointer_feed.hpp"

namespace photorealism {
namespace overlay {
namespace {

bool is_raw_input_message(UINT message) {
    return message == WM_INPUT || message == WM_INPUT_DEVICE_CHANGE;
}

bool is_pointer_message(UINT message) {
    return message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN ||
           message == WM_LBUTTONUP || message == WM_LBUTTONDBLCLK ||
           message == WM_RBUTTONDOWN || message == WM_RBUTTONUP ||
           message == WM_MBUTTONDOWN || message == WM_MBUTTONUP ||
           message == WM_MOUSEWHEEL || message == WM_SETCURSOR;
}

unsigned key_bit(WPARAM key) {
    if (key == VK_UP) {
        return kKeyUp;
    }
    if (key == VK_DOWN) {
        return kKeyDown;
    }
    if (key == VK_LEFT) {
        return kKeyLeft;
    }
    if (key == VK_RIGHT) {
        return kKeyRight;
    }
    if (key == VK_RETURN || key == VK_SPACE) {
        return kKeyEnter;
    }
    if (key == VK_TAB) {
        return kKeyTab;
    }
    return 0;
}

bool is_keyboard_message(UINT message) {
    return message == WM_KEYDOWN || message == WM_KEYUP ||
           message == WM_SYSKEYDOWN || message == WM_SYSKEYUP ||
           message == WM_CHAR;
}

LRESULT CALLBACK overlay_window_proc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    InputHook& hook = input_hook();
    if (hook.handle(message, wparam, lparam)) {
        return 0;
    }
    WNDPROC original = hook.original();
    if (original == nullptr) {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    return CallWindowProcW(original, window, message, wparam, lparam);
}

}

InputHook& input_hook() {
    static InputHook hook;
    return hook;
}

bool InputHook::install(HWND window) {
    if (window == nullptr) {
        return false;
    }
    if (original_ != nullptr && window_ == window) {
        return true;
    }
    remove();

    const LONG_PTR previous = SetWindowLongPtrW(
        window,
        GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&overlay_window_proc));
    if (previous == 0) {
        log_message(
            "Overlay nao conseguiu instalar o WndProc: %lu.", GetLastError());
        return false;
    }
    window_ = window;
    original_ = reinterpret_cast<WNDPROC>(previous);
    log_message("Overlay instalou o WndProc na janela do jogo.");
    return true;
}

void InputHook::remove() {
    if (original_ == nullptr || window_ == nullptr) {
        return;
    }
    const LONG_PTR current = GetWindowLongPtrW(window_, GWLP_WNDPROC);
    if (current == reinterpret_cast<LONG_PTR>(&overlay_window_proc)) {
        SetWindowLongPtrW(
            window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_));
    }
    window_ = nullptr;
    original_ = nullptr;
    reset_events();
}

void InputHook::reset_events() {
    held_.store(false, std::memory_order_release);
    pressed_.store(0, std::memory_order_release);
    released_.store(0, std::memory_order_release);
    wheel_.store(0, std::memory_order_release);
    raw_input_.store(0, std::memory_order_release);
    keys_.store(0, std::memory_order_release);
}

void InputHook::set_capturing(bool capturing) {
    if (capturing_.exchange(capturing, std::memory_order_acq_rel) == capturing) {
        return;
    }
    if (capturing) {
        pointer_feed().reset();
    } else {
        log_message(
            "Menu bloqueou %u mensagens de entrada bruta enquanto esteve aberto.",
            raw_input_.load(std::memory_order_acquire));
    }
    reset_events();
}

bool InputHook::handle(UINT message, WPARAM wparam, LPARAM lparam) {
    (void)lparam;
    if (!capturing()) {
        return false;
    }
    if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) {
        held_.store(true, std::memory_order_release);
        pressed_.fetch_add(1, std::memory_order_acq_rel);
        return true;
    }
    if (message == WM_LBUTTONUP) {
        held_.store(false, std::memory_order_release);
        released_.fetch_add(1, std::memory_order_acq_rel);
        return true;
    }
    if (message == WM_MOUSEWHEEL) {
        wheel_.fetch_add(
            GET_WHEEL_DELTA_WPARAM(wparam), std::memory_order_acq_rel);
        return true;
    }
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
        keys_.fetch_or(key_bit(wparam), std::memory_order_acq_rel);
        return true;
    }
    if (is_raw_input_message(message)) {
        raw_input_.fetch_add(1, std::memory_order_acq_rel);
        return true;
    }
    return is_pointer_message(message) || is_keyboard_message(message);
}

unsigned InputHook::poll_keys() {
    return keys_.exchange(0, std::memory_order_acq_rel);
}

PointerState InputHook::poll() {
    PointerState state;
    state.down = held_.load(std::memory_order_acquire);
    state.pressed = pressed_.exchange(0, std::memory_order_acq_rel) > 0;
    state.released = released_.exchange(0, std::memory_order_acq_rel) > 0;
    state.wheel =
        static_cast<float>(wheel_.exchange(0, std::memory_order_acq_rel)) /
        static_cast<float>(WHEEL_DELTA);

    POINT point = {};
    if (window_ == nullptr || GetCursorPos(&point) == 0) {
        return state;
    }
    if (ScreenToClient(window_, &point) == 0) {
        return state;
    }
    state.x = static_cast<float>(point.x);
    state.y = static_cast<float>(point.y);
    state.inside = true;
    return state;
}

}
}
