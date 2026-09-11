#include "../runtime.hpp"
#include "input.hpp"

extern "C" __declspec(dllexport) int photorealism_menu_capturing() {
    return photorealism::overlay::input_hook().capturing() ? 1 : 0;
}

extern "C" __declspec(dllexport) void photorealism_log_line(const char* text) {
    if (text == nullptr) {
        return;
    }
    photorealism::log_message("%s", text);
}
