#pragma once

#include <windows.h>

namespace photorealism {
namespace dinput {

bool menu_is_capturing();
void gate_log(const char* format, ...);
void forward_mouse(int source, LONG dx, LONG dy, LONG wheel, int buttons);

}
}
