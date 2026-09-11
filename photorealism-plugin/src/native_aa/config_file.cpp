#include "config_file.hpp"

#include "path_utils.hpp"

#include <cwchar>

namespace photorealism {
namespace native_aa {
namespace {

constexpr std::size_t kMaximumConfigBytes = 2u * 1024u * 1024u;

bool sibling_path(
    const wchar_t* config_path, const wchar_t* name, wchar_t* output) {
    std::wcsncpy(output, config_path, MAX_PATH - 1);
    if (!keep_directory_of(output)) {
        return false;
    }
    return append_path(output, MAX_PATH, name);
}

bool write_all(HANDLE file, const std::string& contents) {
    DWORD written = 0;
    const BOOL ok = WriteFile(
        file,
        contents.data(),
        static_cast<DWORD>(contents.size()),
        &written,
        nullptr);
    return ok != FALSE && static_cast<std::size_t>(written) == contents.size();
}

}

bool read_file(const wchar_t* path, std::string* contents) {
    HANDLE file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER size = {};
    const bool usable = GetFileSizeEx(file, &size) && size.QuadPart > 0 &&
                        size.QuadPart <=
                            static_cast<LONGLONG>(kMaximumConfigBytes);
    if (!usable) {
        CloseHandle(file);
        return false;
    }

    contents->resize(static_cast<std::size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL ok = ReadFile(
        file,
        contents->data(),
        static_cast<DWORD>(contents->size()),
        &read,
        nullptr);
    CloseHandle(file);
    return ok != FALSE && static_cast<std::size_t>(read) == contents->size();
}

bool write_atomic(const wchar_t* config_path, const std::string& contents) {
    wchar_t temporary[MAX_PATH] = {};
    if (!sibling_path(
            config_path, L"config.photorealism-aa.tmp", temporary)) {
        return false;
    }

    HANDLE file = CreateFileW(
        temporary,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    const bool wrote = write_all(file, contents);
    FlushFileBuffers(file);
    CloseHandle(file);

    const bool replaced = wrote && MoveFileExW(
        temporary,
        config_path,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (replaced) {
        return true;
    }
    DeleteFileW(temporary);
    return false;
}

bool make_backup(const wchar_t* config_path) {
    wchar_t backup[MAX_PATH] = {};
    if (!sibling_path(
            config_path,
            L"config.photorealism-native-aa.backup.cfg",
            backup)) {
        return false;
    }
    return CopyFileW(config_path, backup, TRUE) != FALSE ||
           GetLastError() == ERROR_FILE_EXISTS;
}

}
}
