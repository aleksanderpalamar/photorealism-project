#include "section_table.hpp"

#include "grade_fields.hpp"
#include "text_utils.hpp"

#include <windows.h>

#include <cstddef>

namespace photorealism {
namespace {


constexpr ModuleField kDepthFields[] = {
    {"near_plane", &Settings::depth_near_plane},
    {"preview_distance", &Settings::depth_preview_distance},
    {"vertical_fov", &Settings::depth_vertical_fov},
};

constexpr ModuleField kSsaoFields[] = {
    {"radius", &Settings::ssao_radius},
    {"intensity", &Settings::ssao_intensity},
    {"bias", &Settings::ssao_bias},
    {"fade_start", &Settings::ssao_fade_start},
    {"fade_end", &Settings::ssao_fade_end},
    {"edge_rejection", &Settings::ssao_edge_rejection},
};

constexpr ModuleField kSsaoRefinementFields[] = {
    {"highlight_start", &Settings::ssao_highlight_start},
    {"highlight_end", &Settings::ssao_highlight_end},
    {"highlight_ao_floor", &Settings::ssao_highlight_ao_floor},
};

constexpr ModuleField kSsaoInteriorFields[] = {
    {"near_start", &Settings::ssao_interior_near_start},
    {"near_end", &Settings::ssao_interior_near_end},
    {"radius", &Settings::ssao_interior_radius},
    {"intensity", &Settings::ssao_interior_intensity},
    {"bias", &Settings::ssao_interior_bias},
    {"edge_rejection", &Settings::ssao_interior_edge_rejection},
};

constexpr ModuleField kTemporalFields[] = {
    {"history_weight", &Settings::temporal_history_weight},
    {"depth_rejection", &Settings::temporal_depth_rejection},
    {"color_rejection", &Settings::temporal_color_rejection},
};

constexpr ModuleField kBloomFields[] = {
    {"threshold", &Settings::bloom_threshold},
    {"knee", &Settings::bloom_knee},
    {"intensity", &Settings::bloom_intensity},
    {"radius", &Settings::bloom_radius},
};

constexpr ModuleField kSceneObserverFields[] = {
    {"interval_frames", &Settings::scene_observer_interval_frames},
    {"log_seconds", &Settings::scene_observer_log_seconds},
};

constexpr ModuleField kConditionFields[] = {
    {"time_constant_seconds", &Settings::condition_time_constant_seconds},
    {"log_seconds", &Settings::condition_log_seconds},
    {"daylight_median_low", &Settings::condition_daylight_median_low},
    {"daylight_median_high", &Settings::condition_daylight_median_high},
    {"overcast_saturation_low", &Settings::condition_overcast_saturation_low},
    {"overcast_saturation_high", &Settings::condition_overcast_saturation_high},
    {"minimum_dynamic_range", &Settings::condition_minimum_dynamic_range},
    {"sun_temperature", &Settings::condition_sun_temperature},
    {"sun_tint", &Settings::condition_sun_tint},
    {"rain_temperature", &Settings::condition_rain_temperature},
    {"rain_tint", &Settings::condition_rain_tint},
    {"night_temperature", &Settings::condition_night_temperature},
    {"night_tint", &Settings::condition_night_tint},
};


template <std::size_t N>
constexpr std::size_t count_of(const ModuleField (&)[N]) {
    return N;
}

const SectionSpec kSections[] = {
    {"plugin", &Settings::enabled, nullptr, nullptr, 0},
    {"base.0.1.2", nullptr, &CalibrationStack::base, nullptr, 0},
    {"module.visual.0.2.0", nullptr, &CalibrationStack::visual_0_2, nullptr, 0},
    {"module.rain_overcast.0.3.0", nullptr,
     &CalibrationStack::rain_overcast_0_3, nullptr, 0},
    {"depth.0.6.4", nullptr, nullptr, kDepthFields, count_of(kDepthFields)},
    {"module.ssao.0.7.0", &Settings::ssao_enabled, nullptr,
     kSsaoFields, count_of(kSsaoFields)},
    {"module.ssao_refinement.0.8.0", &Settings::ssao_refinement_enabled,
     nullptr, kSsaoRefinementFields, count_of(kSsaoRefinementFields)},
    {"module.ssao_interior.0.9.0", &Settings::ssao_interior_enabled,
     nullptr, kSsaoInteriorFields, count_of(kSsaoInteriorFields)},
    {"module.temporal.0.10.0", &Settings::temporal_enabled, nullptr,
     kTemporalFields, count_of(kTemporalFields)},
    {"module.bloom.0.17.0", &Settings::bloom_enabled, nullptr,
     kBloomFields, count_of(kBloomFields)},
    {"module.scene_observer.0.18.0", &Settings::scene_observer_enabled,
     nullptr, kSceneObserverFields, count_of(kSceneObserverFields)},
    {"module.condition_adaptation.0.19.0",
     &Settings::condition_adaptation_enabled, nullptr, kConditionFields,
     count_of(kConditionFields)},
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

void apply_setting(
    CalibrationStack* stack,
    const SectionSpec* section,
    const char* key,
    const char* value) {
    if (section == nullptr) {
        return;
    }
    if (section->layer != nullptr) {
        apply_layer_setting(&(stack->*(section->layer)), key, value);
        return;
    }
    if (section->flag != nullptr && _stricmp(key, "enabled") == 0) {
        stack->modules.*(section->flag) = config_text::parse_bool(value);
        return;
    }
    for (std::size_t i = 0; i < section->field_count; ++i) {
        if (_stricmp(section->fields[i].key, key) != 0) {
            continue;
        }
        stack->modules.*(section->fields[i].member) = config_text::to_number(value);
        return;
    }
}
}
