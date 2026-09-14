#include "setting_binding.hpp"

namespace photorealism {
namespace overlay {

const SettingBinding kProfileBindings[] = {
    {"Perfil photorealism", BindingKind::Toggle, nullptr, &Settings::photorealism_profile_enabled, 0.0f, 0.0f, 0, false, false},
    {"Conjunto de tom", BindingKind::Slider, &Settings::profile_tonemap_set, nullptr, 1.0f, 5.0f, 0, false, false},
    {"Exposicao noturna", BindingKind::Slider, &Settings::profile_night_exposure, nullptr, -4.0f, 4.0f, 2, false, true},
    {"Escala do SSAO", BindingKind::Slider, &Settings::ssao_intensity_scale, nullptr, 0.0f, 4.0f, 2, false, true},
    {"Pre-exposicao*", BindingKind::Slider, &Settings::profile_pre_exposure, nullptr, -4.0f, 4.0f, 2, false, true},
    {"Pre-contraste*", BindingKind::Slider, &Settings::profile_pre_contrast, nullptr, -1.0f, 1.0f, 2, false, true},
    {"Contraste dinamico*", BindingKind::Slider, &Settings::profile_dynamic_contrast, nullptr, 0.0f, 2.0f, 2, false, true},
    {"Luz de interior*", BindingKind::Slider, &Settings::profile_lighting_interior, nullptr, 0.0f, 1.0f, 2, false, true},
    {"Usar luz de interior*", BindingKind::Slider, &Settings::profile_use_interior_lighting, nullptr, 0.0f, 1.0f, 0, false, true},
    {"Saturacao do albedo*", BindingKind::Slider, &Settings::profile_surface_albedo_saturation, nullptr, 0.0f, 2.0f, 2, false, true},
    {"Normais da estrada*", BindingKind::Slider, &Settings::profile_roads_normal_intensity, nullptr, 0.0f, 10.0f, 2, false, true},
    {"Normais padrao*", BindingKind::Slider, &Settings::profile_roads_default_normals, nullptr, 0.0f, 1.0f, 0, false, true},
    {"SSS*", BindingKind::Slider, &Settings::profile_use_sss, nullptr, 0.0f, 1.0f, 0, false, true},
    {"Espelhos do jogo*", BindingKind::Slider, &Settings::profile_use_default_mirrors, nullptr, 0.0f, 1.0f, 0, false, true},
    {"Motion blur*", BindingKind::Slider, &Settings::profile_use_motion_blur, nullptr, 0.0f, 1.0f, 0, false, true},
    {"Intensidade do blur*", BindingKind::Slider, &Settings::profile_motion_blur_intensity, nullptr, 0.0f, 100.0f, 0, false, true},
    {"Espessura das folhas*", BindingKind::Slider, &Settings::profile_vegetation_leaves_thickness, nullptr, 0.0f, 1.0f, 2, false, true},
    {"Espessura da grama*", BindingKind::Slider, &Settings::profile_vegetation_grass_thickness, nullptr, 0.0f, 1.0f, 2, false, true},
    {"Chuva do jogo*", BindingKind::Slider, &Settings::profile_use_default_rain, nullptr, 0.0f, 1.0f, 0, false, true},
    {"FXAA*", BindingKind::Slider, &Settings::profile_fxaa, nullptr, 0.0f, 1.0f, 0, false, true},
    {"SSAO meia resolucao*", BindingKind::Slider, &Settings::profile_use_half_res_ssao, nullptr, 0.0f, 1.0f, 0, false, true},
    {"TAA*", BindingKind::Slider, &Settings::profile_taa, nullptr, 0.0f, 10.0f, 0, false, true},
    {"Nivel do TAA*", BindingKind::Slider, &Settings::profile_taa_level, nullptr, 0.0f, 10.0f, 0, false, true},
    {"Preset DLSS*", BindingKind::Slider, &Settings::profile_dlss_preset, nullptr, 0.0f, 10.0f, 0, false, true},
    {"Tecla mostrar*", BindingKind::Slider, &Settings::profile_hide_show_key, nullptr, 0.0f, 1024.0f, 0, false, true},
    {"Preset de cor*", BindingKind::Slider, &Settings::profile_color_preset, nullptr, 0.0f, 10.0f, 0, false, true},
    {"Brilho extra*", BindingKind::Slider, &Settings::profile_color_preset_extra_brightness, nullptr, -1.0f, 1.0f, 2, false, true},
    {"Operador de tom*", BindingKind::Slider, &Settings::profile_tonemap_operator, nullptr, 0.0f, 10.0f, 0, false, true},
    {"Operador de tom A*", BindingKind::Slider, &Settings::profile_tonemap_operator_a, nullptr, 0.0f, 10.0f, 0, false, true},
    {"Metodo de iluminacao*", BindingKind::Slider, &Settings::profile_lighting_method, nullptr, 0.0f, 3.0f, 0, false, true},
    {"Preset do SSAO*", BindingKind::Slider, &Settings::profile_ssao_preset, nullptr, 0.0f, 10.0f, 0, false, true},
    {"Detalhe do SSAO*", BindingKind::Slider, &Settings::profile_ssao_detail_quality, nullptr, 0.0f, 10.0f, 0, false, true},
    {"Qualidade global*", BindingKind::Slider, &Settings::profile_global_quality, nullptr, 0.0f, 10.0f, 0, false, true},
};

const std::size_t kProfileBindingCount =
    sizeof(kProfileBindings) / sizeof(kProfileBindings[0]);

bool binding_switches_profile(const SettingBinding& binding) {
    return binding.flag == &Settings::photorealism_profile_enabled ||
           binding.number == &Settings::profile_tonemap_set;
}

}
}
