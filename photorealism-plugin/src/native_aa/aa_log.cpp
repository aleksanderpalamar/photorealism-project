#include "aa_log.hpp"

#include "path_utils.hpp"

#include <cstdarg>
#include <cstdio>
#include <cwchar>

namespace photorealism {
namespace native_aa {
namespace {

bool resolve_log_path(HMODULE module, wchar_t* path, std::size_t capacity) {
    if (module == nullptr || GetModuleFileNameW(module, path, MAX_PATH) == 0) {
        return false;
    }
    wchar_t* separator = std::wcsrchr(path, L'\\');
    if (separator == nullptr) {
        return false;
    }
    *separator = L'\0';
    if (!append_path(path, capacity, L"\\photorealism-plugin")) {
        return false;
    }
    CreateDirectoryW(path, nullptr);
    return append_path(path, capacity, L"\\photorealism-aa-config.log");
}

int format_line(char* line, std::size_t capacity, const char* message) {
    SYSTEMTIME time = {};
    GetLocalTime(&time);
    return std::snprintf(
        line,
        capacity,
        "[%02u:%02u:%02u.%03u] %s\r\n",
        static_cast<unsigned>(time.wHour),
        static_cast<unsigned>(time.wMinute),
        static_cast<unsigned>(time.wSecond),
        static_cast<unsigned>(time.wMilliseconds),
        message);
}

void append_line(const wchar_t* path, const char* line, int length) {
    HANDLE file = CreateFileW(
        path,
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
    WriteFile(file, line, static_cast<DWORD>(length), &written, nullptr);
    CloseHandle(file);
}

}

void log_config(HMODULE module, const char* format, ...) {
    wchar_t path[MAX_PATH] = {};
    if (!resolve_log_path(module, path, MAX_PATH)) {
        return;
    }

    char message[1536] = {};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    char line[1792] = {};
    const int length = format_line(line, sizeof(line), message);
    if (length <= 0) {
        return;
    }
    const int clamped =
        length < static_cast<int>(sizeof(line)) ? length : sizeof(line);
    append_line(path, line, clamped);
}

}
}
