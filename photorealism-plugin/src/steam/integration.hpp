#pragma once

#include "steam_api.hpp"

namespace photorealism {
namespace steam {

bool ensure_integration();
const ScreenshotsApi& api();
void return_to_native_capture(const char* reason, bool trigger);

}
}
