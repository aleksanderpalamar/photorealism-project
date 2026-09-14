#include "setting_binding.hpp"

namespace photorealism {
namespace overlay {
namespace {

constexpr float kSliderKeyboardSteps = 100.0f;

float clamped(const SettingBinding& binding, float value) {
    if (value < binding.minimum) {
        return binding.minimum;
    }
    return value > binding.maximum ? binding.maximum : value;
}

}

float binding_value(const SettingBinding& binding, const Settings& settings) {
    if (binding.number == nullptr) {
        return 0.0f;
    }
    return settings.*(binding.number);
}

void set_binding_value(
    const SettingBinding& binding, Settings* settings, float value) {
    if (binding.number == nullptr || settings == nullptr) {
        return;
    }
    settings->*(binding.number) = value;
}

bool binding_flag(const SettingBinding& binding, const Settings& settings) {
    if (binding.flag != nullptr) {
        return settings.*(binding.flag);
    }
    return binding.number != nullptr && settings.*(binding.number) != 0.0f;
}

void set_binding_flag(
    const SettingBinding& binding, Settings* settings, bool value) {
    if (settings == nullptr) {
        return;
    }
    if (binding.flag != nullptr) {
        settings->*(binding.flag) = value;
        return;
    }
    set_binding_value(binding, settings, value ? 1.0f : 0.0f);
}

std::size_t choice_count(const SettingBinding& binding) {
    const float span = binding.maximum - binding.minimum;
    return span > 0.0f ? static_cast<std::size_t>(span + 0.5f) + 1 : 1;
}

std::size_t choice_index(const SettingBinding& binding, float value) {
    const float offset = clamped(binding, value) - binding.minimum;
    const std::size_t index = static_cast<std::size_t>(offset + 0.5f);
    const std::size_t last = choice_count(binding) - 1;
    return index > last ? last : index;
}

bool choice_selectable(const SettingBinding& binding, std::size_t index) {
    const float value = binding.minimum + static_cast<float>(index);
    return value <= binding.selectable_maximum + 0.5f;
}

float stepped_binding_value(
    const SettingBinding& binding, float current, int direction) {
    if (binding.kind != BindingKind::Choice) {
        const float step =
            (binding.maximum - binding.minimum) / kSliderKeyboardSteps;
        return quantized_binding_value(
            binding, current + step * static_cast<float>(direction));
    }
    const float base =
        binding.minimum + static_cast<float>(choice_index(binding, current));
    const float moved = base + static_cast<float>(direction);
    const float highest = binding.selectable_maximum;
    return moved > highest ? highest : quantized_binding_value(binding, moved);
}

float cycled_binding_value(const SettingBinding& binding, float current) {
    const float next =
        binding.minimum + static_cast<float>(choice_index(binding, current) + 1);
    return next > binding.selectable_maximum + 0.5f ? binding.minimum : next;
}

float quantized_binding_value(const SettingBinding& binding, float value) {
    float scale = 1.0f;
    for (int digit = 0; digit < binding.decimals; ++digit) {
        scale *= 10.0f;
    }
    const float scaled = clamped(binding, value) * scale;
    const float rounded = scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f;
    return static_cast<float>(static_cast<long long>(rounded)) / scale;
}

bool binding_edits_tonemap(const SettingBinding& binding) {
    return binding.tonemap;
}

bool row_is_selectable(const MenuRow& row) {
    return row.kind != RowKind::Separator;
}

}
}
