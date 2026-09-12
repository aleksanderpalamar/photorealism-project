#include "../runtime.hpp"
#include "input.hpp"
#include "pointer_feed.hpp"

extern "C" __declspec(dllexport) int photorealism_menu_capturing() {
    return photorealism::overlay::input_hook().capturing() ? 1 : 0;
}

extern "C" __declspec(dllexport) void photorealism_log_line(const char* text) {
    if (text == nullptr) {
        return;
    }
    photorealism::log_message("%s", text);
}

extern "C" __declspec(dllexport) void photorealism_menu_mouse(
    int source, long dx, long dy, long wheel, int buttons) {
    photorealism::overlay::pointer_feed().push(
        static_cast<photorealism::overlay::PointerSource>(source),
        dx,
        dy,
        wheel,
        buttons);
}
