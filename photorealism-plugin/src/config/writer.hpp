#pragma once

#include <string>

namespace photorealism {
namespace config_writer {

bool set_value(
    std::string* contents,
    const char* section,
    const char* key,
    const char* value);

}
}
