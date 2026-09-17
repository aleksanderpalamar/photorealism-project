#include "capture_folder.hpp"

#include "../runtime.hpp"

#include <windows.h>

#include <cstdio>

namespace photorealism {
namespace frame_capture {

bool create_capture_folder(std::wstring* folder, std::string* name) {
    SYSTEMTIME time = {};
    GetLocalTime(&time);
    char stamp[64] = {};
    std::snprintf(
        stamp, sizeof(stamp), "captura-quadro-%04u%02u%02u-%02u%02u%02u",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    *name = stamp;
    *folder = plugin_root();
    folder->append(L"\\");
    folder->append(name->begin(), name->end());
    return CreateDirectoryW(folder->c_str(), nullptr) != FALSE ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

bool write_text_file(const std::wstring& folder, const wchar_t* file, const std::string& text) {
    std::wstring path = folder + L"\\" + file;
    FILE* handle = _wfopen(path.c_str(), L"wb");
    if (handle == nullptr) {
        return false;
    }
    const bool written = std::fwrite(text.data(), 1, text.size(), handle) == text.size();
    return std::fclose(handle) == 0 && written;
}

}
}
