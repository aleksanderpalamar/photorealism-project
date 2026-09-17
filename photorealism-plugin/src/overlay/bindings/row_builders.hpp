#pragma once

#include "setting_binding.hpp"

namespace photorealism {
namespace overlay {

constexpr SettingBinding flag_toggle(const char* label, bool Settings::*flag) {
    SettingBinding binding;
    binding.label = label;
    binding.kind = BindingKind::Toggle;
    binding.flag = flag;
    return binding;
}

constexpr SettingBinding toggle(const char* label, float Settings::*number) {
    SettingBinding binding;
    binding.label = label;
    binding.kind = BindingKind::Toggle;
    binding.number = number;
    binding.maximum = 1.0f;
    return binding;
}

constexpr SettingBinding slider(
    const char* label, float Settings::*number, float minimum, float maximum,
    int decimals) {
    SettingBinding binding;
    binding.label = label;
    binding.number = number;
    binding.minimum = minimum;
    binding.maximum = maximum;
    binding.decimals = decimals;
    return binding;
}

constexpr SettingBinding tone_slider(
    const char* label, float Settings::*number, float minimum, float maximum,
    int decimals) {
    SettingBinding binding = slider(label, number, minimum, maximum, decimals);
    binding.tonemap = true;
    return binding;
}

constexpr SettingBinding choice(
    const char* label, float Settings::*number, float maximum,
    const char* const* choices) {
    SettingBinding binding;
    binding.label = label;
    binding.kind = BindingKind::Choice;
    binding.number = number;
    binding.maximum = maximum;
    binding.choices = choices;
    binding.selectable_maximum = maximum;
    return binding;
}

constexpr SettingBinding limited_choice(
    const char* label, float Settings::*number, float maximum,
    float selectable_maximum, const char* const* choices) {
    SettingBinding binding = choice(label, number, maximum, choices);
    binding.selectable_maximum = selectable_maximum;
    return binding;
}

constexpr MenuRow setting_row(const SettingBinding& binding) {
    MenuRow row;
    row.kind = RowKind::Setting;
    row.first = binding;
    return row;
}

constexpr MenuRow pair_row(const SettingBinding& first, const SettingBinding& second) {
    MenuRow row;
    row.kind = RowKind::Pair;
    row.first = first;
    row.second = second;
    return row;
}

constexpr MenuRow action_row(RowKind kind, const char* label, std::size_t target) {
    MenuRow row;
    row.kind = kind;
    row.first.label = label;
    row.target = target;
    return row;
}

constexpr MenuRow link_row(const char* label, std::size_t target) {
    return action_row(RowKind::Link, label, target);
}

constexpr MenuRow separator_row() {
    return MenuRow{};
}

}
}
