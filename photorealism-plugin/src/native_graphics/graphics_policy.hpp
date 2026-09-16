#pragma once

#include "graphics_settings.hpp"

#include <windows.h>

namespace photorealism {
namespace native_graphics {

GraphicsPolicy read_native_graphics_policy(HMODULE proxy_module);

}
}
