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
    bool (*reader)(CalibrationStack* stack, const char* key, const char* value);
};

const SectionSpec* find_section(const char* name);
bool locate_number(
    float Settings::*member, const char** section, const char** key);
bool locate_flag(bool Settings::*flag, const char** section);
void apply_setting(
    CalibrationStack* stack,
    const SectionSpec* section,
    const char* key,
    const char* value);

}
