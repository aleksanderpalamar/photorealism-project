#pragma once

#include "input_state.hpp"

#include <atomic>
#include <windows.h>

namespace photorealism {
namespace overlay {

class InputHook {
  public:
    bool install(HWND window);
    void remove();

    bool installed() const { return original_ != nullptr; }
    HWND window() const { return window_; }
    WNDPROC original() const { return original_; }

    void set_capturing(bool capturing);
    bool capturing() const { return capturing_.load(std::memory_order_acquire); }

    bool handle(UINT message, WPARAM wparam, LPARAM lparam);
    PointerState poll();
    unsigned poll_keys();

  private:
    void reset_events();

    HWND window_ = nullptr;
    WNDPROC original_ = nullptr;
    std::atomic<bool> capturing_{false};
    std::atomic<bool> held_{false};
    std::atomic<unsigned> pressed_{0};
    std::atomic<unsigned> released_{0};
    std::atomic<int> wheel_{0};
    std::atomic<unsigned> raw_input_{0};
    std::atomic<unsigned> keys_{0};
};

InputHook& input_hook();

}
}
