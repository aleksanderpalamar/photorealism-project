#include "runtime.hpp"

#include <cstdarg>
#include <cstdio>
#include <cwchar>

namespace photorealism {
namespace {

HMODULE g_module = nullptr;
INIT_ONCE g_paths_once = INIT_ONCE_STATIC_INIT;
wchar_t g_module_directory[MAX_PATH] = {};
wchar_t g_plugin_root[MAX_PATH] = {};
wchar_t g_config_path[MAX_PATH] = {};
wchar_t g_shader_path[MAX_PATH] = {};
wchar_t g_depth_preview_shader_path[MAX_PATH] = {};
wchar_t g_ssao_shader_path[MAX_PATH] = {};
wchar_t g_temporal_shader_path[MAX_PATH] = {};
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

BOOL CALLBACK initialize_paths(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t module_path[MAX_PATH] = {};
    if (g_module == nullptr ||
        GetModuleFileNameW(g_module, module_path, MAX_PATH) == 0) {
        std::wcsncpy(g_module_directory, L".", MAX_PATH - 1);
    } else {
        wchar_t* separator = std::wcsrchr(module_path, L'\\');
        if (separator != nullptr) {
            *separator = L'\0';
        }
        std::wcsncpy(g_module_directory, module_path, MAX_PATH - 1);
    }

    std::wcsncpy(g_plugin_root, g_module_directory, MAX_PATH - 1);
    append_path(g_plugin_root, MAX_PATH, L"\\photorealism-plugin");
    CreateDirectoryW(g_plugin_root, nullptr);

    std::wcsncpy(g_config_path, g_plugin_root, MAX_PATH - 1);
    append_path(g_config_path, MAX_PATH, L"\\photorealism-plugin.cfg");

    std::wcsncpy(g_shader_path, g_plugin_root, MAX_PATH - 1);
    append_path(g_shader_path, MAX_PATH, L"\\shaders\\photorealism.hlsl");

    std::wcsncpy(g_depth_preview_shader_path, g_plugin_root, MAX_PATH - 1);
    append_path(
        g_depth_preview_shader_path,
        MAX_PATH,
        L"\\shaders\\depth-preview.hlsl");

    std::wcsncpy(g_ssao_shader_path, g_plugin_root, MAX_PATH - 1);
    append_path(g_ssao_shader_path, MAX_PATH, L"\\shaders\\ssao.hlsl");

    std::wcsncpy(g_temporal_shader_path, g_plugin_root, MAX_PATH - 1);
    append_path(
        g_temporal_shader_path, MAX_PATH, L"\\shaders\\temporal.hlsl");

    std::wcsncpy(g_log_path, g_plugin_root, MAX_PATH - 1);
    append_path(g_log_path, MAX_PATH, L"\\photorealism-plugin.log");
    return TRUE;
}

void ensure_paths() {
    InitOnceExecuteOnce(&g_paths_once, initialize_paths, nullptr, nullptr);
}

}  // namespace

void set_module(HMODULE module) {
    g_module = module;
}

const wchar_t* module_directory() {
    ensure_paths();
    return g_module_directory;
}

const wchar_t* plugin_root() {
    ensure_paths();
    return g_plugin_root;
}

const wchar_t* config_path() {
    ensure_paths();
    return g_config_path;
}

const wchar_t* shader_path() {
    ensure_paths();
    return g_shader_path;
}

const wchar_t* depth_preview_shader_path() {
    ensure_paths();
    return g_depth_preview_shader_path;
}

const wchar_t* ssao_shader_path() {
    ensure_paths();
    return g_ssao_shader_path;
}

const wchar_t* temporal_shader_path() {
    ensure_paths();
    return g_temporal_shader_path;
}

void log_message(const char* format, ...) {
    ensure_paths();

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
    const DWORD bytes_to_write = static_cast<DWORD>(
        length < static_cast<int>(sizeof(line))
            ? length
            : static_cast<int>(sizeof(line) - 1));

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
    WriteFile(file, line, bytes_to_write, &written, nullptr);
    CloseHandle(file);
}

}  // namespace photorealism
