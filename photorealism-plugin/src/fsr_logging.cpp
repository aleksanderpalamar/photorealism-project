#include "fsr_logging.hpp"

#include <cstdarg>
#include <cstdio>
#include <cwchar>

namespace {

HMODULE g_log_module = nullptr;
INIT_ONCE g_log_path_once = INIT_ONCE_STATIC_INIT;
wchar_t g_log_path[MAX_PATH] = {};

bool append_path(wchar_t* destination, size_t capacity, const wchar_t* suffix) {
    const size_t used = std::wcslen(destination);
    const size_t extra = std::wcslen(suffix);
    if (used + extra + 1 > capacity) {
        return false;
    }
    std::wmemcpy(destination + used, suffix, extra + 1);
    return true;
}

BOOL CALLBACK initialize_log_path(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t module_path[MAX_PATH] = {};
    if (g_log_module == nullptr ||
        GetModuleFileNameW(g_log_module, module_path, MAX_PATH) == 0) {
        std::wcsncpy(module_path, L".", MAX_PATH - 1);
    } else {
        wchar_t* separator = std::wcsrchr(module_path, L'\\');
        if (separator != nullptr) {
            *separator = L'\0';
        }
    }

    if (!append_path(module_path, MAX_PATH, L"\\photorealism-plugin")) {
        return FALSE;
    }
    CreateDirectoryW(module_path, nullptr);
    std::wcsncpy(g_log_path, module_path, MAX_PATH - 1);
    return append_path(g_log_path, MAX_PATH, L"\\photorealism-fsr.log")
               ? TRUE
               : FALSE;
}

}  // namespace

void fsr_log_set_module(HMODULE module) {
    g_log_module = module;
}

void log_message(const char* format, ...) {
    if (!InitOnceExecuteOnce(
            &g_log_path_once, initialize_log_path, nullptr, nullptr)) {
        return;
    }

    char message[2048] = {};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    SYSTEMTIME time = {};
    GetLocalTime(&time);
    char line[2304] = {};
    const int length = std::snprintf(
        line,
        sizeof(line),
        "[%02u:%02u:%02u.%03u] %s\r\n",
        static_cast<unsigned>(time.wHour),
        static_cast<unsigned>(time.wMinute),
        static_cast<unsigned>(time.wSecond),
        static_cast<unsigned>(time.wMilliseconds),
        message);
    if (length <= 0) {
        return;
    }

    OutputDebugStringA(line);
    HANDLE file = CreateFileW(
        g_log_path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    const DWORD bytes = static_cast<DWORD>(
        length < static_cast<int>(sizeof(line))
            ? length
            : static_cast<int>(sizeof(line) - 1));
    WriteFile(file, line, bytes, &written, nullptr);
    CloseHandle(file);
}
