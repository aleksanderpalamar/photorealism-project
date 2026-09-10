#pragma once

#include <windows.h>

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace photorealism {
namespace config_text {

inline float clamp_value(float value, float minimum, float maximum) {
    const float low = value < minimum ? minimum : value;
    return low > maximum ? maximum : low;
}

inline float to_number(const char* value) {
    return static_cast<float>(std::strtod(value, nullptr));
}

inline bool parse_bool(const char* value) {
    return _stricmp(value, "true") == 0 || _stricmp(value, "yes") == 0 ||
           std::atoi(value) != 0;
}

inline char* trim(char* text) {
    while (*text != '\0' && std::isspace(static_cast<unsigned char>(*text))) {
        ++text;
    }
    char* end = text + std::strlen(text);
    while (end > text && std::isspace(static_cast<unsigned char>(end[-1]))) {
        --end;
    }
    *end = '\0';
    return text;
}
}
}
