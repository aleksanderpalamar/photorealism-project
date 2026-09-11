#include "path_utils.hpp"

#include <cwchar>

namespace photorealism {
namespace native_aa {

bool append_path(wchar_t* path, std::size_t capacity, const wchar_t* suffix) {
    const std::size_t used = std::wcslen(path);
    const std::size_t added = std::wcslen(suffix);
    if (used + added + 1 > capacity) {
        return false;
    }
    std::wmemcpy(path + used, suffix, added + 1);
    return true;
}

bool keep_directory_of(wchar_t* path) {
    wchar_t* separator = std::wcsrchr(path, L'\\');
    if (separator == nullptr) {
        return false;
    }
    separator[1] = L'\0';
    return true;
}

}
}
