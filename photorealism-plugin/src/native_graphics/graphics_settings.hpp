#pragma once

#include <cstddef>
#include <string>

namespace photorealism {
namespace native_graphics {

struct GraphicsSetting {
    const char* key;
    const char* fallback;
};

constexpr GraphicsSetting kSettings[] = {
    {"r_ssao", "0"},
};

constexpr std::size_t kSettingCount =
    sizeof(kSettings) / sizeof(kSettings[0]);

constexpr const char* kNativeGraphicsSection = "native_graphics.0.1.0";
constexpr const char* kAbsent = "ausente";

struct GraphicsPolicy {
    bool manage;
    std::string desired[kSettingCount];
};

struct GraphicsSnapshot {
    std::string detected[kSettingCount];
};

}
}
