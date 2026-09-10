#include "hook_audit.hpp"

#include "hook.hpp"
#include "../runtime.hpp"
#include "hook_state.hpp"
#include "swap_chain_hooks.hpp"
#include "module_names.hpp"

#include <windows.h>

namespace photorealism {

using namespace hook_state;

void log_present_entry(
    const char* phase,
    const char* method,
    void** entry,
    const void* replacement,
    const void* downstream) {
    void* current = entry != nullptr ? *entry : nullptr;
    char current_owner[MAX_PATH * 3] = {};
    char downstream_owner[MAX_PATH * 3] = {};
    log_message(
        "Auditoria Present 0.11.0: phase=%s method=%s entry=%p "
        "current=%p current_owner=%s ours=%p downstream=%p "
        "downstream_owner=%s state=%s.",
        phase != nullptr ? phase : "unknown",
        method,
        static_cast<void*>(entry),
        current,
        module_name_for_address(current, current_owner, sizeof(current_owner)),
        replacement,
        downstream,
        module_name_for_address(
            downstream, downstream_owner, sizeof(downstream_owner)),
        current == replacement ? "nosso-hook-externo" : "substituido-ou-encadeado");
}


void audit_swap_chain_hook_chain(const char* phase) {
    void** present_entry =
        g_present_vtable_entry.load(std::memory_order_acquire);
    if (present_entry != nullptr) {
        log_present_entry(
            phase,
            "Present",
            present_entry,
            reinterpret_cast<void*>(&hooked_present),
            reinterpret_cast<void*>(
                g_original_present.load(std::memory_order_acquire)));
    }
    void** present1_entry =
        g_present1_vtable_entry.load(std::memory_order_acquire);
    if (present1_entry != nullptr) {
        log_present_entry(
            phase,
            "Present1",
            present1_entry,
            reinterpret_cast<void*>(&hooked_present1),
            reinterpret_cast<void*>(
                g_original_present1.load(std::memory_order_acquire)));
    }
}
}
