#pragma once

#include <atomic>

#include <windows.h>

namespace photorealism {

bool patch_vtable_slot(void** entry, void* replacement, void** original_out);

template <typename Function>
bool replace_vtable_entry(
    void** entry, void* replacement, std::atomic<Function>* original_storage) {
    if (original_storage->load(std::memory_order_acquire) != nullptr) {
        return true;
    }
    void* original = nullptr;
    if (!patch_vtable_slot(entry, replacement, &original)) {
        return false;
    }
    original_storage->store(
        reinterpret_cast<Function>(original), std::memory_order_release);
    return true;
}

}
