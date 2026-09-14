#pragma once

#include "../../config/settings.hpp"

#include <cstddef>

namespace photorealism {
namespace overlay {

enum class BindingKind {
    Toggle,
    Slider,
};

struct SettingBinding {
    const char* label;
    BindingKind kind;
    float Settings::*number;
    bool Settings::*flag;
    float minimum;
    float maximum;
    int decimals;
    bool grade;
    bool inert;
};

struct SettingPage {
    const char* tab;
    const char* title;
    const SettingBinding* items;
    std::size_t count;
};

extern const SettingBinding kGradeBindings[];
extern const std::size_t kGradeBindingCount;
extern const SettingBinding kRenderBindings[];
extern const std::size_t kRenderBindingCount;
extern const SettingBinding kConditionBindings[];
extern const std::size_t kConditionBindingCount;
extern const SettingBinding kUpscaleBindings[];
extern const std::size_t kUpscaleBindingCount;
extern const SettingBinding kObserverBindings[];
extern const std::size_t kObserverBindingCount;
extern const SettingBinding kProfileBindings[];
extern const std::size_t kProfileBindingCount;

const SettingPage* setting_pages();
std::size_t setting_page_count();

float binding_value(const SettingBinding& binding, const Settings& settings);
void set_binding_value(
    const SettingBinding& binding, Settings* settings, float value);
bool binding_flag(const SettingBinding& binding, const Settings& settings);
void set_binding_flag(
    const SettingBinding& binding, Settings* settings, bool value);
bool binding_touches_observer(const SettingBinding& binding);
bool binding_switches_profile(const SettingBinding& binding);

}
}
