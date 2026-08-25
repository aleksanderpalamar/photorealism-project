#pragma once

#include <dxgi.h>

namespace photorealism {

// Official Steamworks screenshot hand-off. No keyboard interception is used.
void observe_postprocessed_frame(IDXGISwapChain* swap_chain);
void prepare_steam_screenshot_resize();
void shutdown_steam_screenshots();

}  // namespace photorealism
