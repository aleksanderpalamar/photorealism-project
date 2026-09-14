#pragma once

#include "settings.hpp"

namespace photorealism {

struct ModuleField {
    const char* key;
    float Settings::*member;
};

}
