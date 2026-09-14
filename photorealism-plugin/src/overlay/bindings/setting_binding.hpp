#pragma once

#include "../../config/settings.hpp"

#include <cstddef>

namespace photorealism {
namespace overlay {

enum class BindingKind {
    Toggle,
    Slider,
    Choice,
};

struct SettingBinding {
    const char* label = nullptr;
    BindingKind kind = BindingKind::Slider;
    float Settings::*number = nullptr;
    bool Settings::*flag = nullptr;
    float minimum = 0.0f;
    float maximum = 0.0f;
    int decimals = 0;
    bool tonemap = false;
    const char* const* choices = nullptr;
    float selectable_maximum = 0.0f;
};

enum class RowKind {
    Setting,
    Pair,
    Link,
    Separator,
    Hide,
    Restore,
};

struct MenuRow {
    RowKind kind = RowKind::Separator;
    SettingBinding first;
    SettingBinding second;
    std::size_t target = 0;
};

struct SettingPage {
    const char* title;
    const MenuRow* rows;
    std::size_t count;
    std::size_t parent;
    bool upscale_status;
};

constexpr std::size_t kPageMain = 0;
constexpr std::size_t kPageAntiAliasing = 1;
constexpr std::size_t kPageRendering = 2;
constexpr std::size_t kPageColors = 3;
constexpr std::size_t kPageObjects = 4;
constexpr std::size_t kPageSurface = 5;
constexpr std::size_t kPageRoads = 6;
constexpr std::size_t kPageVegetation = 7;
constexpr std::size_t kPageUpscale = 8;

const SettingPage* setting_pages();
std::size_t setting_page_count();

float binding_value(const SettingBinding& binding, const Settings& settings);
void set_binding_value(
    const SettingBinding& binding, Settings* settings, float value);
bool binding_flag(const SettingBinding& binding, const Settings& settings);
void set_binding_flag(
    const SettingBinding& binding, Settings* settings, bool value);
float stepped_binding_value(
    const SettingBinding& binding, float current, int direction);
float cycled_binding_value(const SettingBinding& binding, float current);
float quantized_binding_value(const SettingBinding& binding, float value);
std::size_t choice_count(const SettingBinding& binding);
std::size_t choice_index(const SettingBinding& binding, float value);
bool choice_selectable(const SettingBinding& binding, std::size_t index);
bool binding_edits_tonemap(const SettingBinding& binding);
bool row_is_selectable(const MenuRow& row);

}
}
