#pragma once

#include <windows.h>

#include <string>

namespace photorealism {
namespace native_aa {

bool read_file(const wchar_t* path, std::string* contents);
bool write_atomic(const wchar_t* config_path, const std::string& contents);
bool make_backup(const wchar_t* config_path);

}
}
