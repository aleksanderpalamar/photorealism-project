#include "section_table.hpp"

#include "profile_fields.hpp"
#include "text_utils.hpp"

#include <windows.h>

#include <cstddef>

namespace photorealism {
namespace {

constexpr ModuleField kBloomFields[] = {
    {"threshold", &Settings::bloom_threshold},
    {"knee", &Settings::bloom_knee},
    {"intensity", &Settings::bloom_intensity},
    {"radius", &Settings::bloom_radius},
};

constexpr ModuleField kFsrFields[] = {
    {"render_scale", &Settings::fsr_render_scale},
    {"sharpness", &Settings::fsr_sharpness},
    {"grain", &Settings::fsr_grain},
};

constexpr ModuleField kWetSurfaceFields[] = {
    {"amount", &Settings::wet_roads_amount},
    {"ripple", &Settings::wet_roads_ripple},
    {"gloss", &Settings::wet_roads_gloss},
    {"darkening", &Settings::wet_roads_darkening},
    {"floor", &Settings::wet_roads_floor},
};

constexpr ModuleField kSceneObserverFields[] = {
    {"interval_frames", &Settings::scene_observer_interval_frames},
    {"log_seconds", &Settings::scene_observer_log_seconds},
};

template <std::size_t N>
constexpr std::size_t count_of(const ModuleField (&)[N]) {
    return N;
}

bool read_tonemap_key(Settings* settings, const char* key, const char* value) {
    return apply_tonemap_key(settings->tonemap_sets, key, value);
}

const SectionSpec kSections[] = {
    {"plugin", &Settings::enabled, nullptr, 0, nullptr},
    {kProfileSection, nullptr, kProfileFields, kProfileFieldCount,
     read_tonemap_key},
    {"module.bloom.0.17.0", &Settings::bloom_enabled, kBloomFields,
     count_of(kBloomFields), nullptr},
    {"module.fsr.0.21.0", &Settings::fsr_enabled, kFsrFields,
     count_of(kFsrFields), nullptr},
    {"module.wet_surface.0.25.0", &Settings::wet_surface_enabled,
     kWetSurfaceFields, count_of(kWetSurfaceFields), nullptr},
    {"module.scene_observer.0.18.0", &Settings::scene_observer_enabled,
     kSceneObserverFields, count_of(kSceneObserverFields), nullptr},
};

constexpr std::size_t kSectionCount = sizeof(kSections) / sizeof(kSections[0]);

}

const SectionSpec* find_section(const char* name) {
    for (std::size_t i = 0; i < kSectionCount; ++i) {
        if (_stricmp(kSections[i].name, name) == 0) {
            return &kSections[i];
        }
    }
    return nullptr;
}

bool locate_number(
    float Settings::*member, const char** section, const char** key) {
    for (std::size_t index = 0; index < kSectionCount; ++index) {
        const SectionSpec& spec = kSections[index];
        for (std::size_t field = 0; field < spec.field_count; ++field) {
            if (spec.fields[field].member != member) {
                continue;
            }
            *section = spec.name;
            *key = spec.fields[field].key;
            return true;
        }
    }
    return false;
}

bool locate_flag(bool Settings::*flag, const char** section) {
    for (std::size_t index = 0; index < kSectionCount; ++index) {
        if (kSections[index].flag != flag) {
            continue;
        }
        *section = kSections[index].name;
        return true;
    }
    return false;
}

void apply_setting(
    Settings* settings,
    const SectionSpec* section,
    const char* key,
    const char* value) {
    if (section == nullptr) {
        return;
    }
    if (section->flag != nullptr && _stricmp(key, "enabled") == 0) {
        settings->*(section->flag) = config_text::parse_bool(value);
        return;
    }
    if (section->reader != nullptr && section->reader(settings, key, value)) {
        return;
    }
    for (std::size_t i = 0; i < section->field_count; ++i) {
        if (_stricmp(section->fields[i].key, key) != 0) {
            continue;
        }
        settings->*(section->fields[i].member) = config_text::to_number(value);
        return;
    }
}

}
