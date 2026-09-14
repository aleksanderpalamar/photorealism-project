#include "row_builders.hpp"

namespace photorealism {
namespace overlay {
namespace {

constexpr const char* kBack = "Voltar";

constexpr const char* kLightingChoices[] = {
    "Iluminacao A", "Iluminacao B", "Iluminacao C", "Iluminacao D (padrao)"};
constexpr const char* kQualityChoices[] = {
    "Qualidade alta", "Qualidade media", "Qualidade baixa"};
constexpr const char* kAntiAliasingChoices[] = {
    "Desligado", "Anti-aliasing temporal", "Anti-aliasing temporal nitido",
    "DLAA (sem suporte)", "DLSS qualidade (sem suporte)",
    "DLSS equilibrado (sem suporte)", "DLSS desempenho (sem suporte)"};

constexpr const char* kSsaoPresetChoices[] = {
    "SSAO suave", "SSAO medio", "SSAO forte"};
constexpr const char* kSsaoDetailChoices[] = {
    "Detalhe alto", "Detalhe medio", "Detalhe baixo"};

constexpr MenuRow kMainRows[] = {
    pair_row(
        choice("", &Settings::profile_lighting_method, 3.0f, kLightingChoices),
        choice("", &Settings::profile_global_quality, 2.0f, kQualityChoices)),
    link_row("Anti-aliasing / Motion blur", kPageAntiAliasing),
    link_row("Renderizacao / Iluminacao", kPageRendering),
    link_row("Cores / Tom", kPageColors),
    link_row("Objetos", kPageObjects),
    link_row("Upscale FSR", kPageUpscale),
    separator_row(),
    action_row(RowKind::Hide, "Mostrar / esconder: Ctrl+P", kPageMain),
    action_row(RowKind::Restore, "Restaurar padroes", kPageMain),
};

constexpr MenuRow kAntiAliasingRows[] = {
    setting_row(limited_choice("", &Settings::profile_taa, 6.0f, 2.0f, kAntiAliasingChoices)),
    setting_row(toggle("FXAA", &Settings::profile_fxaa)),
    setting_row(slider("Nitidez", &Settings::profile_sharpness, 0.0f, 10.0f, 0)),
    setting_row(slider("Nitidez das bordas", &Settings::profile_sharpen_edges, 0.0f, 10.0f, 0)),
    separator_row(),
    setting_row(toggle("Motion blur", &Settings::profile_use_motion_blur)),
    setting_row(slider("Intensidade do blur", &Settings::profile_motion_blur_intensity, 0.0f, 20.0f, 0)),
    link_row(kBack, kPageMain),
};

constexpr MenuRow kRenderingRows[] = {
    setting_row(toggle("SSAO em baixa resolucao", &Settings::profile_use_half_res_ssao)),
    pair_row(
        choice("", &Settings::profile_ssao_preset, 2.0f, kSsaoPresetChoices),
        choice("", &Settings::profile_ssao_detail_quality, 2.0f, kSsaoDetailChoices)),
    setting_row(slider("Intensidade do SSAO", &Settings::profile_ssao_intensity, 0.0f, 4.0f, 2)),
    separator_row(),
    setting_row(slider("Intensidade do interior", &Settings::profile_lighting_interior, 0.0f, 2.0f, 2)),
    setting_row(toggle("Luz de interior", &Settings::profile_use_interior_lighting)),
    setting_row(toggle("Espelhos do jogo", &Settings::profile_use_default_mirrors)),
    setting_row(toggle("Chuva do jogo", &Settings::profile_use_default_rain)),
    setting_row(toggle("SSS", &Settings::profile_use_sss)),
    link_row(kBack, kPageMain),
};

constexpr MenuRow kColorRows[] = {
    setting_row(tone_slider("Temperatura", &Settings::temperature, 3000.0f, 9000.0f, 0)),
    setting_row(tone_slider("Pre-exposicao", &Settings::profile_pre_exposure, -2.0f, 2.0f, 2)),
    setting_row(tone_slider("Pre-contraste", &Settings::profile_pre_contrast, -1.0f, 1.0f, 2)),
    setting_row(tone_slider("Exposicao", &Settings::exposure, -2.0f, 2.0f, 2)),
    setting_row(tone_slider("Saturacao", &Settings::saturation, 0.0f, 2.0f, 2)),
    setting_row(tone_slider("Contraste", &Settings::contrast, 0.5f, 1.5f, 2)),
    setting_row(tone_slider("Vibracao", &Settings::vibrance, -1.0f, 1.0f, 2)),
    setting_row(tone_slider("Sombras", &Settings::shadows, -1.0f, 1.0f, 2)),
    setting_row(tone_slider("Altas luzes", &Settings::highlights, -1.0f, 1.0f, 2)),
    setting_row(tone_slider("Pretos", &Settings::blacks, -1.0f, 1.0f, 2)),
    setting_row(tone_slider("Brancos", &Settings::whites, -1.0f, 1.0f, 2)),
    setting_row(tone_slider("Exposicao noturna", &Settings::profile_night_exposure, -4.0f, 4.0f, 2)),
    link_row(kBack, kPageMain),
};

constexpr MenuRow kObjectRows[] = {
    link_row("Superficie", kPageSurface),
    link_row("Estradas", kPageRoads),
    link_row("Vegetacao", kPageVegetation),
    link_row(kBack, kPageMain),
};

constexpr MenuRow kSurfaceRows[] = {
    setting_row(slider("Saturacao do albedo", &Settings::profile_surface_albedo_saturation, 0.0f, 2.0f, 2)),
    link_row(kBack, kPageObjects),
};

constexpr MenuRow kRoadRows[] = {
    setting_row(toggle("Normais padrao", &Settings::profile_roads_default_normals)),
    setting_row(slider("Intensidade das normais", &Settings::profile_roads_normal_intensity, 0.0f, 10.0f, 2)),
    link_row(kBack, kPageObjects),
};

constexpr MenuRow kVegetationRows[] = {
    setting_row(slider("Espessura das folhas", &Settings::profile_vegetation_leaves_thickness, 0.0f, 1.0f, 2)),
    setting_row(slider("Espessura da grama", &Settings::profile_vegetation_grass_thickness, 0.0f, 1.0f, 2)),
    link_row(kBack, kPageObjects),
};

constexpr MenuRow kUpscaleRows[] = {
    setting_row(flag_toggle("Upscale FSR", &Settings::fsr_enabled)),
    setting_row(slider("Escala de render", &Settings::fsr_render_scale, 0.5f, 1.0f, 4)),
    setting_row(slider("Nitidez do RCAS", &Settings::fsr_sharpness, 0.0f, 1.0f, 2)),
    setting_row(slider("Granulacao LFGA", &Settings::fsr_grain, 0.0f, 1.0f, 2)),
    link_row(kBack, kPageMain),
};

template <std::size_t N>
constexpr SettingPage page(
    const char* title, const MenuRow (&rows)[N], std::size_t parent,
    bool upscale_status) {
    return SettingPage{title, rows, N, parent, upscale_status};
}

constexpr SettingPage kPages[] = {
    page("Inicio", kMainRows, kPageMain, false),
    page("Anti-aliasing / Motion blur", kAntiAliasingRows, kPageMain, false),
    page("Renderizacao / Iluminacao", kRenderingRows, kPageMain, false),
    page("Cores / Tom", kColorRows, kPageMain, false),
    page("Objetos", kObjectRows, kPageMain, false),
    page("Superficie", kSurfaceRows, kPageObjects, false),
    page("Estradas", kRoadRows, kPageObjects, false),
    page("Vegetacao", kVegetationRows, kPageObjects, false),
    page("Upscale FSR", kUpscaleRows, kPageMain, true),
};

}

const SettingPage* setting_pages() {
    return kPages;
}

std::size_t setting_page_count() {
    return sizeof(kPages) / sizeof(kPages[0]);
}

}
}
