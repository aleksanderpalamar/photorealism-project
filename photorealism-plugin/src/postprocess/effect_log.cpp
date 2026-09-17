#include "effect_log.hpp"

#include "../config/effect_logging.hpp"
#include "../runtime.hpp"

#include <windows.h>

#include <cstring>

namespace photorealism {
namespace {

constexpr unsigned long long kMinimumIntervalMs = 250ull;

}

void EffectLog::report(const Settings& settings) {
    char text[sizeof(last_)] = {};
    format_effect_settings(settings, text, sizeof(text));
    const unsigned long long now = GetTickCount64();
    const bool changed = std::strcmp(text, last_) != 0;
    if (!changed || now - last_tick_ < kMinimumIntervalMs) {
        return;
    }
    std::memcpy(last_, text, sizeof(last_));
    last_tick_ = now;
    log_message("%s", text);
}

}
