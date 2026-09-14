#pragma once

#include "../native_aa/config_text.hpp"

#include <string>

namespace photorealism {
namespace fsr {

constexpr const char* kAbsentScale = "ausente";
constexpr const char* kScaleKeyX = "r_scale_x";
constexpr const char* kScaleKeyY = "r_scale_y";

struct ScaleAxes {
    std::string x;
    std::string y;
};

inline bool axes_empty(const ScaleAxes& axes) {
    return axes.x.empty() && axes.y.empty();
}

inline std::string strip_spaces(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string();
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

inline ScaleAxes parse_saved_scale(const std::string& saved) {
    const std::string clean = strip_spaces(saved);
    const std::size_t split = clean.find_first_of(" \t\r\n");
    if (split == std::string::npos) {
        return ScaleAxes{clean, clean};
    }
    return ScaleAxes{clean.substr(0, split), strip_spaces(clean.substr(split))};
}

inline std::string format_saved_scale(const ScaleAxes& axes) {
    return axes.x + "\n" + axes.y + "\n";
}

inline ScaleAxes read_game_scale(const std::string& game_config) {
    return ScaleAxes{
        aa_config::config_value(game_config, kScaleKeyX),
        aa_config::config_value(game_config, kScaleKeyY)};
}

inline bool write_scale_axis(
    std::string* game_config, const char* key, const std::string& wanted) {
    const std::string current = aa_config::config_value(*game_config, key);
    if (wanted.empty() || wanted == kAbsentScale) {
        return false;
    }
    if (current == wanted) {
        return false;
    }
    return aa_config::set_config_value(game_config, key, wanted.c_str());
}

inline bool write_game_scale(std::string* game_config, const ScaleAxes& wanted) {
    const bool wrote_x = write_scale_axis(game_config, kScaleKeyX, wanted.x);
    const bool wrote_y = write_scale_axis(game_config, kScaleKeyY, wanted.y);
    return wrote_x || wrote_y;
}

}
}
