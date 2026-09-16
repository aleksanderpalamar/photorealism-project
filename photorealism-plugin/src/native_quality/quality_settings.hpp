#pragma once

#include <cstddef>
#include <string>

namespace photorealism {
namespace native_quality {

struct QualitySetting {
    const char* key;
    const char* high;
    const char* medium;
    const char* low;
    bool lower_is_better;
};

constexpr QualitySetting kSettings[] = {
    {"r_texture_detail", "0", "1", "2", true},
    {"r_anisotropy_factor", "4", "2", "0", false},
    {"r_normal_maps", "1", "1", "0", false},
    {"r_sun_shadow_quality", "3", "2", "1", false},
    {"r_sun_shadow_texture_size", "4096", "2048", "1024", false},
    {"r_far_shadow_disable", "0", "0", "1", true},
    {"r_interior_shadow", "1", "1", "0", false},
    {"r_fake_shadows", "2", "2", "1", false},
    {"r_cloud_shadows", "1", "1", "0", false},
    {"r_deferred_mirrors", "2", "1", "0", false},
    {"r_mirror_scale_x", "2", "1", "1", false},
    {"r_mirror_scale_y", "2", "1", "1", false},
    {"r_mirror_view_distance", "120", "100", "80", false},
    {"g_reflection", "2", "1", "0", false},
    {"g_reflection_scale", "2", "1", "1", false},
    {"r_sunshafts", "1", "0", "0", false},
    {"g_veg_detail", "2", "1", "0", false},
    {"g_grass_density", "2", "1", "0", false},
    {"g_pedestrian", "1", "1", "0", false},
    {"g_light_distance_factor", "2", "1", "0", false},
    {"g_lod_factor_traffic", "2", "1", "1", false},
    {"g_lod_factor_parked", "2", "1", "1", false},
    {"g_lod_factor_pedestrian", "2", "1", "1", false},
};

constexpr std::size_t kSettingCount = sizeof(kSettings) / sizeof(kSettings[0]);

constexpr const char* kNativeQualitySection = "native_quality.0.25.1";
constexpr const char* kProfileSectionName = "profile.photorealism.0.23.0";
constexpr const char* kGlobalQualityKey = "global_quality";
constexpr const char* kAbsent = "ausente";

enum class QualityLevel : unsigned {
    high = 0,
    medium = 1,
    low = 2,
};

constexpr const char* level_name(QualityLevel level) {
    return level == QualityLevel::high     ? "alta"
           : level == QualityLevel::medium ? "media"
                                           : "baixa";
}

constexpr const char* value_for(
    const QualitySetting& setting, QualityLevel level) {
    return level == QualityLevel::high     ? setting.high
           : level == QualityLevel::medium ? setting.medium
                                           : setting.low;
}

struct QualityPolicy {
    bool manage;
    QualityLevel level;
    std::string desired[kSettingCount];
};

struct QualitySnapshot {
    std::string detected[kSettingCount];
};

}
}
