#include "setting_binding.hpp"

namespace photorealism {
namespace overlay {

const SettingBinding kUpscaleBindings[] = {
    {"Upscale FSR", BindingKind::Toggle, nullptr, &Settings::fsr_enabled, 0.0f, 0.0f, 0, false, false},
    {"Escala de render", BindingKind::Slider, &Settings::fsr_render_scale, nullptr, 0.5f, 1.0f, 4, false, false},
    {"Nitidez do RCAS", BindingKind::Slider, &Settings::fsr_sharpness, nullptr, 0.0f, 1.0f, 2, false, false},
    {"Granulacao LFGA", BindingKind::Slider, &Settings::fsr_grain, nullptr, 0.0f, 1.0f, 2, false, false},
};

const std::size_t kUpscaleBindingCount =
    sizeof(kUpscaleBindings) / sizeof(kUpscaleBindings[0]);

}
}
