#include "config.hpp"

#include "../runtime.hpp"
#include "defaults.hpp"
#include "limits.hpp"
#include "logging.hpp"
#include "profile_state.hpp"
#include "section_table.hpp"
#include "text_utils.hpp"

#include <windows.h>

#include <cstdio>
#include <cstring>

namespace photorealism {
namespace {

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

void read_settings_from_file(FILE* file, Settings* settings) {
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
        apply_setting(settings, section, key, value);
    }
}

}

void finish_settings(Settings* settings) {
    apply_active_tonemap(settings);
    derive_profile_controls(settings);
    apply_limits(settings);
}

Settings default_settings() {
    Settings settings = reference_settings();
    finish_settings(&settings);
    return settings;
}

Settings default_settings_with_lighting(float lighting_method) {
    Settings settings = reference_settings();
    settings.profile_lighting_method = lighting_method;
    finish_settings(&settings);
    return settings;
}

bool read_settings(Settings* settings) {
    if (settings == nullptr) {
        return false;
    }
    *settings = reference_settings();
    FILE* file = _wfopen(config_path(), L"rb");
    if (file != nullptr) {
        read_settings_from_file(file, settings);
        std::fclose(file);
    }
    finish_settings(settings);
    return file != nullptr;
}

bool load_settings(Settings* settings) {
    if (settings == nullptr) {
        return false;
    }
    const bool found = read_settings(settings);
    if (!found) {
        log_message("Configuracao ausente; usando os valores internos do perfil.");
    }
    log_settings(*settings);
    return found;
}

}
