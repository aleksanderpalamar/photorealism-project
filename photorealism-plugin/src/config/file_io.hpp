#pragma once

#include <string>
#include <windows.h>

namespace photorealism {
namespace config_io {

bool read_file(const wchar_t* path, std::string* contents);

bool write_atomic(
    const wchar_t* path,
    const wchar_t* temporary_name,
    const std::string& contents);

bool copy_once(const wchar_t* path, const wchar_t* backup_name);

}
}
