#pragma once

#include <cstdint>

namespace photorealism::steam_screenshot {

// Steam normally emits one ScreenshotRequested_t per physical hotkey press.
// Proton/overlay callback delivery is treated defensively: while a capture is
// in flight no second request can be admitted, and an immediate duplicate
// after completion remains coalesced for a short bounded interval.
constexpr std::uint64_t kDuplicateRequestWindowMs = 750;

constexpr bool request_is_duplicate(
    std::uint64_t now_ms,
    std::uint64_t last_accepted_ms,
    bool capture_in_flight) {
    if (capture_in_flight) {
        return true;
    }
    if (last_accepted_ms == 0 || now_ms < last_accepted_ms) {
        return false;
    }
    return now_ms - last_accepted_ms < kDuplicateRequestWindowMs;
}

}  // namespace photorealism::steam_screenshot
