#pragma once

#include <cstddef>
#include <string>

namespace photorealism {
namespace native_aa {

struct AaSetting {
    const char* key;
    const char* fallback;
};

constexpr AaSetting kSettings[] = {
    {"r_aa", "6"},
    {"r_taa_tuning", "0"},
    {"r_taa_luma_sharpen", "1.5"},
    {"r_taa_modulated_drr_strength", "0.0"},
};

constexpr std::size_t kSettingCount =
    sizeof(kSettings) / sizeof(kSettings[0]);

constexpr const char* kNativeAaSection = "native_aa.0.12.2";
constexpr const char* kAbsent = "ausente";

struct AaPolicy {
    bool manage;
    std::string desired[kSettingCount];
};

struct AaSnapshot {
    std::string detected[kSettingCount];
};

}
}
