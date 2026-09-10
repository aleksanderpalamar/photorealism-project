#pragma once

#include "calibration.hpp"
#include "settings.hpp"

#include <cstddef>

namespace photorealism {

struct ModuleField {
    const char* key;
    float Settings::*member;
};

struct SectionSpec {
    const char* name;
    bool Settings::*flag;
    CalibrationLayer CalibrationStack::*layer;
    const ModuleField* fields;
    std::size_t field_count;
};

const SectionSpec* find_section(const char* name);
void apply_setting(
    CalibrationStack* stack,
    const SectionSpec* section,
    const char* key,
    const char* value);

}
