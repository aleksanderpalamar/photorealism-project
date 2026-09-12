#include "menu_gate.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <windows.h>

namespace photorealism {
namespace dinput {
namespace {

using CapturingFunction = int (*)();
using LogFunction = void (*)(const char*);
using MouseFunction = void (*)(int, LONG, LONG, LONG, int);

template <typename Function>
Function resolve(std::atomic<Function>* cache, const char* name) {
    Function found = cache->load(std::memory_order_acquire);
    if (found != nullptr) {
        return found;
    }
    HMODULE graphics = GetModuleHandleW(L"dxgi.dll");
    if (graphics == nullptr) {
        return nullptr;
    }
    found = reinterpret_cast<Function>(GetProcAddress(graphics, name));
    if (found == nullptr) {
        return nullptr;
    }
    cache->store(found, std::memory_order_release);
    return found;
}

std::atomic<CapturingFunction> g_capturing{nullptr};
std::atomic<LogFunction> g_log{nullptr};
std::atomic<MouseFunction> g_mouse{nullptr};

}

bool menu_is_capturing() {
    CapturingFunction query =
        resolve(&g_capturing, "photorealism_menu_capturing");
    return query != nullptr && query() != 0;
}

void gate_log(const char* format, ...) {
    LogFunction sink = resolve(&g_log, "photorealism_log_line");
    if (sink == nullptr) {
        return;
    }
    char line[512] = {};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    sink(line);
}

void forward_mouse(int source, LONG dx, LONG dy, LONG wheel, int buttons) {
    MouseFunction sink = resolve(&g_mouse, "photorealism_menu_mouse");
    if (sink == nullptr) {
        return;
    }
    sink(source, dx, dy, wheel, buttons);
}

}
}
