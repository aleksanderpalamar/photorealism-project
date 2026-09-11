#include "setting_binding.hpp"

namespace photorealism {
namespace overlay {

const SettingBinding kRenderBindings[] = {
    {"Oclusao de ambiente", BindingKind::Toggle, nullptr, &Settings::ssao_enabled, 0.0f, 0.0f, 0, false, false},
    {"Raio", BindingKind::Slider, &Settings::ssao_radius, nullptr, 0.05f, 5.0f, 2, false, false},
    {"Intensidade", BindingKind::Slider, &Settings::ssao_intensity, nullptr, 0.0f, 1.0f, 2, false, false},
    {"Vies", BindingKind::Slider, &Settings::ssao_bias, nullptr, 0.0f, 0.5f, 3, false, false},
    {"Inicio do sumico", BindingKind::Slider, &Settings::ssao_fade_start, nullptr, 1.0f, 500.0f, 1, false, false},
    {"Fim do sumico", BindingKind::Slider, &Settings::ssao_fade_end, nullptr, 2.0f, 1000.0f, 1, false, false},
    {"Rejeicao de borda", BindingKind::Slider, &Settings::ssao_edge_rejection, nullptr, 1.05f, 4.0f, 2, false, false},

    {"Refino em altas luzes", BindingKind::Toggle, nullptr, &Settings::ssao_refinement_enabled, 0.0f, 0.0f, 0, false, false},
    {"Inicio do realce", BindingKind::Slider, &Settings::ssao_highlight_start, nullptr, 0.0f, 2.0f, 2, false, false},
    {"Fim do realce", BindingKind::Slider, &Settings::ssao_highlight_end, nullptr, 0.01f, 4.0f, 2, false, false},
    {"Piso de oclusao", BindingKind::Slider, &Settings::ssao_highlight_ao_floor, nullptr, 0.0f, 1.0f, 2, false, false},

    {"Oclusao de interior", BindingKind::Toggle, nullptr, &Settings::ssao_interior_enabled, 0.0f, 0.0f, 0, false, false},
    {"Inicio do perto", BindingKind::Slider, &Settings::ssao_interior_near_start, nullptr, 0.1f, 50.0f, 2, false, false},
    {"Fim do perto", BindingKind::Slider, &Settings::ssao_interior_near_end, nullptr, 0.2f, 100.0f, 2, false, false},
    {"Raio do interior", BindingKind::Slider, &Settings::ssao_interior_radius, nullptr, 0.05f, 5.0f, 2, false, false},
    {"Intensidade do interior", BindingKind::Slider, &Settings::ssao_interior_intensity, nullptr, 0.0f, 1.0f, 2, false, false},
    {"Vies do interior", BindingKind::Slider, &Settings::ssao_interior_bias, nullptr, 0.0f, 0.5f, 3, false, false},
    {"Borda do interior", BindingKind::Slider, &Settings::ssao_interior_edge_rejection, nullptr, 1.05f, 4.0f, 2, false, false},

    {"Resolve temporal", BindingKind::Toggle, nullptr, &Settings::temporal_enabled, 0.0f, 0.0f, 0, false, false},
    {"Peso do historico", BindingKind::Slider, &Settings::temporal_history_weight, nullptr, 0.0f, 0.95f, 2, false, false},
    {"Rejeicao por depth", BindingKind::Slider, &Settings::temporal_depth_rejection, nullptr, 0.001f, 0.5f, 3, false, false},
    {"Rejeicao por cor", BindingKind::Slider, &Settings::temporal_color_rejection, nullptr, 0.005f, 1.0f, 3, false, false},

    {"Bloom", BindingKind::Toggle, nullptr, &Settings::bloom_enabled, 0.0f, 0.0f, 0, false, false},
    {"Intensidade do bloom", BindingKind::Slider, &Settings::bloom_intensity, nullptr, 0.0f, 1.0f, 2, false, false},
    {"Raio do bloom", BindingKind::Slider, &Settings::bloom_radius, nullptr, 0.005f, 0.2f, 3, false, false},
    {"Limiar do bloom", BindingKind::Slider, &Settings::bloom_threshold, nullptr, 0.2f, 0.98f, 2, false, true},
    {"Joelho do bloom", BindingKind::Slider, &Settings::bloom_knee, nullptr, 0.0f, 0.5f, 2, false, true},
};

const std::size_t kRenderBindingCount =
    sizeof(kRenderBindings) / sizeof(kRenderBindings[0]);

}
}
