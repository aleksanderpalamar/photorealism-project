#include "module_names.hpp"

#include <windows.h>

#include <cstdio>

namespace photorealism {

const char* module_name_for_address(
    const void* address, char* output, std::size_t size) {
    if (output == nullptr || size == 0) {
        return "indisponivel";
    }
    output[0] = '\0';
    if (address == nullptr) {
        std::snprintf(output, size, "null");
        return output;
    }
    HMODULE owner = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address),
            &owner) ||
        owner == nullptr) {
        std::snprintf(output, size, "owner-desconhecido");
        return output;
    }
    wchar_t wide_path[MAX_PATH] = {};
    if (GetModuleFileNameW(owner, wide_path, MAX_PATH) == 0 ||
        WideCharToMultiByte(
            CP_UTF8,
            0,
            wide_path,
            -1,
            output,
            static_cast<int>(size),
            nullptr,
            nullptr) == 0) {
        std::snprintf(output, size, "module=%p", static_cast<void*>(owner));
    }
    return output;
}
}
