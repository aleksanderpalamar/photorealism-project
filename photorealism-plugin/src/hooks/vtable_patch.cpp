#include "vtable_patch.hpp"

namespace photorealism {

bool patch_vtable_slot(void** entry, void* replacement, void** original_out) {
    if (entry == nullptr || replacement == nullptr || original_out == nullptr) {
        return false;
    }

    DWORD old_protection = 0;
    if (!VirtualProtect(
            entry, sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protection)) {
        return false;
    }

    *original_out = *entry;
    InterlockedExchangePointer(
        reinterpret_cast<void* volatile*>(entry), replacement);

    DWORD restored_protection = 0;
    VirtualProtect(entry, sizeof(void*), old_protection, &restored_protection);
    FlushInstructionCache(GetCurrentProcess(), entry, sizeof(void*));
    return true;
}

}
