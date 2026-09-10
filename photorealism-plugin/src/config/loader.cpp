#include "config.hpp"

#include "../runtime.hpp"
#include "defaults.hpp"
#include "grade_fields.hpp"
#include "limits.hpp"
#include "logging.hpp"
#include "section_table.hpp"
#include "text_utils.hpp"

#include <windows.h>

#include <cstdio>
#include <cstring>

namespace photorealism {
namespace {

Settings compose_stack(const CalibrationStack& stack) {
    Settings settings = stack.modules;

    if (stack.base.enabled) {
        copy_base_layer(&settings, stack.base);
    }
    add_delta_layer(&settings, stack.visual_0_2);
    add_delta_layer(&settings, stack.rain_overcast_0_3);

    apply_limits(&settings);
    return settings;
}

bool read_section_header(char* content, const SectionSpec** section) {
    if (*content != '[') {
        return false;
    }
    char* closing = std::strchr(content + 1, ']');
    if (closing == nullptr) {
        *section = nullptr;
        return true;
    }
    *closing = '\0';
    *section = find_section(config_text::trim(content + 1));
    return true;
}

bool split_key_value(char* content, char** key, char** value) {
    char* separator = std::strchr(content, '=');
    if (separator == nullptr) {
        return false;
    }
    *separator = '\0';
    *key = config_text::trim(content);
    *value = config_text::trim(separator + 1);
    return true;
}

bool is_ignorable(const char* content) {
    return *content == '\0' || *content == '#' || *content == ';';
}

void read_stack_from_file(FILE* file, CalibrationStack* stack) {
    const SectionSpec* section = nullptr;
    char line[512] = {};
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        char* content = config_text::trim(line);
        if (is_ignorable(content)) {
            continue;
        }
        if (read_section_header(content, &section)) {
            continue;
        }
        char* key = nullptr;
        char* value = nullptr;
        if (!split_key_value(content, &key, &value)) {
            continue;
        }
        apply_setting(stack, section, key, value);
    }
}
}

Settings default_settings() {
    return compose_stack(reference_stack());
}

bool load_settings(Settings* settings) {
    if (settings == nullptr) {
        return false;
    }

    CalibrationStack stack = reference_stack();
    FILE* file = _wfopen(config_path(), L"rb");
    if (file == nullptr) {
        *settings = compose_stack(stack);
        log_message("Configuracao ausente; usando a pilha cumulativa interna.");
        log_stack(stack, *settings);
        return false;
    }

    read_stack_from_file(file, &stack);
    std::fclose(file);

    *settings = compose_stack(stack);
    log_stack(stack, *settings);
    return true;
}
}
