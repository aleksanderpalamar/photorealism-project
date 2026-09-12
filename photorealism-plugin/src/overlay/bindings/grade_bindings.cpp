#include "setting_binding.hpp"

namespace photorealism {
namespace overlay {

const SettingBinding kGradeBindings[] = {
    {"Exposicao", BindingKind::Slider, &Settings::exposure, nullptr, -2.0f, 2.0f, 3, true, false},
    {"Contraste", BindingKind::Slider, &Settings::contrast, nullptr, 0.5f, 1.5f, 3, true, false},
    {"Saturacao", BindingKind::Slider, &Settings::saturation, nullptr, 0.0f, 2.0f, 3, true, false},
    {"Vibracao", BindingKind::Slider, &Settings::vibrance, nullptr, -1.0f, 1.0f, 3, true, false},
    {"Sombras", BindingKind::Slider, &Settings::shadows, nullptr, -1.0f, 1.0f, 3, true, false},
    {"Altas luzes", BindingKind::Slider, &Settings::highlights, nullptr, -1.0f, 1.0f, 3, true, false},
    {"Pretos", BindingKind::Slider, &Settings::blacks, nullptr, -1.0f, 1.0f, 3, true, false},
    {"Brancos", BindingKind::Slider, &Settings::whites, nullptr, -1.0f, 1.0f, 3, true, false},
    {"Contraste local", BindingKind::Slider, &Settings::local_contrast, nullptr, 0.0f, 1.0f, 3, true, false},
    {"Nitidez", BindingKind::Slider, &Settings::sharpness, nullptr, 0.0f, 1.0f, 3, true, false},
    {"Vinheta", BindingKind::Slider, &Settings::vignette, nullptr, 0.0f, 0.5f, 3, true, false},
    {"Joelho de altas luzes", BindingKind::Slider, &Settings::highlight_rolloff, nullptr, 0.0f, 1.0f, 3, true, false},
    {"Piso de preto R", BindingKind::Slider, &Settings::black_lift_r, nullptr, 0.0f, 0.02f, 6, true, false},
    {"Piso de preto G", BindingKind::Slider, &Settings::black_lift_g, nullptr, 0.0f, 0.02f, 6, true, false},
    {"Piso de preto B", BindingKind::Slider, &Settings::black_lift_b, nullptr, 0.0f, 0.02f, 6, true, false},
    {"Temperatura de reserva", BindingKind::Slider, &Settings::temperature, nullptr, 3000.0f, 9000.0f, 0, true, false},
    {"Matiz de reserva", BindingKind::Slider, &Settings::tint, nullptr, -1.0f, 1.0f, 3, true, false},
};

const std::size_t kGradeBindingCount =
    sizeof(kGradeBindings) / sizeof(kGradeBindings[0]);

}
}
