#pragma once

#include "steam_api.hpp"

#include <atomic>
#include <cstdint>

namespace photorealism {
namespace steam {

extern std::atomic<std::uint32_t> g_requests;
extern std::atomic<bool> g_callback_accepting;
extern std::atomic<bool> g_capture_cycle_active;
extern std::atomic<std::uint64_t> g_last_accepted_tick;
extern std::atomic<std::uint64_t> g_accepted_requests;
extern std::atomic<std::uint64_t> g_coalesced_requests;
extern std::atomic<std::uint64_t> g_completed_writes;

CallbackBase* screenshot_callback();

std::uint64_t accepted_requests();
std::uint64_t coalesced_requests();
void reset_gate();

}
}
