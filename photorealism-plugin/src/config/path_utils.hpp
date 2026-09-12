#pragma once

#include <windows.h>

#include <cstddef>

namespace photorealism {
namespace paths {

bool append_path(wchar_t* path, std::size_t capacity, const wchar_t* suffix);
bool keep_directory_of(wchar_t* path);

}
}
