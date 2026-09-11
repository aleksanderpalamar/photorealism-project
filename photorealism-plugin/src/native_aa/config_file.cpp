#include "config_file.hpp"

#include "../config/file_io.hpp"

namespace photorealism {
namespace native_aa {

bool read_file(const wchar_t* path, std::string* contents) {
    return config_io::read_file(path, contents);
}

bool write_atomic(const wchar_t* config_path, const std::string& contents) {
    return config_io::write_atomic(
        config_path, L"config.photorealism-aa.tmp", contents);
}

bool make_backup(const wchar_t* config_path) {
    return config_io::copy_once(
        config_path, L"config.photorealism-native-aa.backup.cfg");
}

}
}
