#include "setting_binding.hpp"

namespace photorealism {
namespace overlay {

const SettingBinding kConditionBindings[] = {
    {"Adaptacao por condicao", BindingKind::Toggle, nullptr, &Settings::condition_adaptation_enabled, 0.0f, 0.0f, 0, false, false},
    {"Constante de tempo (s)", BindingKind::Slider, &Settings::condition_time_constant_seconds, nullptr, 1.0f, 1800.0f, 0, false, false},

    {"Temperatura do sol (K)", BindingKind::Slider, &Settings::condition_sun_temperature, nullptr, 3000.0f, 9000.0f, 0, false, false},
    {"Matiz do sol", BindingKind::Slider, &Settings::condition_sun_tint, nullptr, -1.0f, 1.0f, 3, false, false},
    {"Temperatura da chuva (K)", BindingKind::Slider, &Settings::condition_rain_temperature, nullptr, 3000.0f, 9000.0f, 0, false, false},
    {"Matiz da chuva", BindingKind::Slider, &Settings::condition_rain_tint, nullptr, -1.0f, 1.0f, 3, false, false},
    {"Temperatura da noite (K)", BindingKind::Slider, &Settings::condition_night_temperature, nullptr, 3000.0f, 9000.0f, 0, false, false},
    {"Matiz da noite", BindingKind::Slider, &Settings::condition_night_tint, nullptr, -1.0f, 1.0f, 3, false, false},

    {"Mediana de dia, piso", BindingKind::Slider, &Settings::condition_daylight_median_low, nullptr, 0.0f, 120.0f, 1, false, false},
    {"Mediana de dia, teto", BindingKind::Slider, &Settings::condition_daylight_median_high, nullptr, 1.0f, 200.0f, 1, false, false},
    {"Saturacao nublada, piso", BindingKind::Slider, &Settings::condition_overcast_saturation_low, nullptr, 0.0f, 0.5f, 3, false, false},
    {"Saturacao nublada, teto", BindingKind::Slider, &Settings::condition_overcast_saturation_high, nullptr, 0.01f, 0.6f, 3, false, false},
    {"Faixa dinamica minima", BindingKind::Slider, &Settings::condition_minimum_dynamic_range, nullptr, 0.0f, 255.0f, 1, false, false},
    {"Log da condicao (s)", BindingKind::Slider, &Settings::condition_log_seconds, nullptr, 0.0f, 3600.0f, 0, false, false},
};

const std::size_t kConditionBindingCount =
    sizeof(kConditionBindings) / sizeof(kConditionBindings[0]);

const SettingBinding kObserverBindings[] = {
    {"Observador de cena", BindingKind::Toggle, nullptr, &Settings::scene_observer_enabled, 0.0f, 0.0f, 0, false, false},
    {"Intervalo (quadros)", BindingKind::Slider, &Settings::scene_observer_interval_frames, nullptr, 1.0f, 600.0f, 0, false, false},
    {"Log do observador (s)", BindingKind::Slider, &Settings::scene_observer_log_seconds, nullptr, 0.0f, 3600.0f, 0, false, false},

    {"Plano proximo", BindingKind::Slider, &Settings::depth_near_plane, nullptr, 0.001f, 10.0f, 3, false, false},
    {"Distancia do preview", BindingKind::Slider, &Settings::depth_preview_distance, nullptr, 1.0f, 10000.0f, 0, false, false},
    {"Campo de visao vertical", BindingKind::Slider, &Settings::depth_vertical_fov, nullptr, 20.0f, 140.0f, 1, false, false},
};

const std::size_t kObserverBindingCount =
    sizeof(kObserverBindings) / sizeof(kObserverBindings[0]);

}
}
