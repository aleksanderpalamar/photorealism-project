#pragma once

#include "module_field.hpp"
#include "settings.hpp"

#include <cstddef>

namespace photorealism {

struct SectionSpec {
    const char* name;
    bool Settings::*flag;
    const ModuleField* fields;
    std::size_t field_count;
    bool (*reader)(Settings* settings, const char* key, const char* value);
};

const SectionSpec* find_section(const char* name);
bool locate_number(
    float Settings::*member, const char** section, const char** key);
bool locate_flag(bool Settings::*flag, const char** section);
void apply_setting(
    Settings* settings,
    const SectionSpec* section,
    const char* key,
    const char* value);

}
