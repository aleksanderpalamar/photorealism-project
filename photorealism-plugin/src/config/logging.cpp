#include "logging.hpp"

#include "../runtime.hpp"
#include "text_utils.hpp"

#include <cmath>

namespace photorealism {
namespace {

void log_effective_profile(const Settings& settings) {
    log_message(
        "Perfil efetivo: temperature=%.1f exposure=%.3f contrast=%.3f "
        "saturation=%.3f vibrance=%.3f shadows=%.3f highlights=%.3f "
        "blacks=%.3f whites=%.3f local_contrast=%.3f sharpness=%.3f "
        "vignette=%.3f.",
        settings.temperature, settings.exposure, settings.contrast,
        settings.saturation, settings.vibrance, settings.shadows,
        settings.highlights, settings.blacks, settings.whites,
        settings.local_contrast, settings.sharpness, settings.vignette);

    log_message(
        "Perfil efetivo (cor): tint=%.3f highlight_rolloff=%.3f "
        "black_lift=%.6f/%.6f/%.6f.",
        settings.tint, settings.highlight_rolloff, settings.black_lift_r,
        settings.black_lift_g, settings.black_lift_b);
}

void log_white_balance(const Settings& settings) {
    const float shift =
        config_text::clamp_value((settings.temperature - 6500.0f) / 3500.0f, -1.0f, 1.0f);
    const float tint = config_text::clamp_value(settings.tint, -1.0f, 1.0f);
    const float r = 1.0f - 0.08f * shift - 0.05f * tint;
    const float g = 1.0f + 0.10f * tint;
    const float b = 1.0f + 0.10f * shift - 0.05f * tint;
    const float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    const float divisor = luma > 1e-4f ? luma : 1e-4f;
    const float normalized_luma =
        (0.2126f * (r / divisor) + 0.7152f * (g / divisor) +
         0.0722f * (b / divisor));
    log_message(
        "Balanco de branco 0.18.2: bruto=%.4f/%.4f/%.4f luma_bruta=%.6f "
        "(%+.4f EV) normalizado=%.4f/%.4f/%.4f ganho_luma=%.6f (%+.4f EV).",
        static_cast<double>(r), static_cast<double>(g), static_cast<double>(b),
        static_cast<double>(luma), static_cast<double>(std::log2(luma)),
        static_cast<double>(r / divisor), static_cast<double>(g / divisor),
        static_cast<double>(b / divisor),
        static_cast<double>(normalized_luma),
        static_cast<double>(std::log2(normalized_luma)));
}

void log_modules(const Settings& settings) {
    log_message(
        "Depth linearization 0.6.4: reversed_z=sim near_plane=%.4f "
        "preview_distance=%.1f vertical_fov=%.1f.",
        settings.depth_near_plane, settings.depth_preview_distance,
        settings.depth_vertical_fov);
    log_message(
        "Modulo SSAO 0.7.0: %s samples=8 radius=%.3f intensity=%.3f "
        "bias=%.3f fade=%.1f-%.1f edge_rejection=%.2f.",
        settings.ssao_enabled ? "ativo" : "inativo", settings.ssao_radius,
        settings.ssao_intensity, settings.ssao_bias, settings.ssao_fade_start,
        settings.ssao_fade_end, settings.ssao_edge_rejection);
    log_message(
        "Modulo SSAO refinement 0.8.0: %s samples=16 "
        "highlight_protection=%.2f-%.2f ao_floor=%.2f.",
        settings.ssao_refinement_enabled ? "ativo" : "inativo",
        settings.ssao_highlight_start, settings.ssao_highlight_end,
        settings.ssao_highlight_ao_floor);
    log_message(
        "Modulo SSAO interior 0.9.0: %s faixa=%.1f-%.1fm "
        "radius=%.3f intensity=%.3f bias=%.3f edge_rejection=%.2f.",
        settings.ssao_interior_enabled ? "ativo" : "inativo",
        settings.ssao_interior_near_start, settings.ssao_interior_near_end,
        settings.ssao_interior_radius, settings.ssao_interior_intensity,
        settings.ssao_interior_bias, settings.ssao_interior_edge_rejection);
    log_message(
        "Modulo temporal 0.10.0: %s history_weight=%.2f "
        "depth_rejection=%.3f color_rejection=%.3f.",
        settings.temporal_enabled ? "ativo" : "inativo",
        settings.temporal_history_weight, settings.temporal_depth_rejection,
        settings.temporal_color_rejection);
    log_message(
        "Modulo bloom 0.17.0: %s threshold=%.3f knee=%.3f intensity=%.3f "
        "radius=%.4f (licenca artistica; so o limiar e medido).",
        settings.bloom_enabled ? "ativo" : "inativo", settings.bloom_threshold,
        settings.bloom_knee, settings.bloom_intensity, settings.bloom_radius);
    log_message(
        "Modulo observador de cena 0.18.0: %s intervalo=%.0f frames "
        "log=%.0fs (mede o frame pre-grade; nao altera a imagem).",
        settings.scene_observer_enabled ? "ativo" : "inativo",
        settings.scene_observer_interval_frames,
        settings.scene_observer_log_seconds);
    log_message(
        "Modulo adaptacao por condicao 0.19.0: %s tau=%.0fs log=%.0fs "
        "dia=%.1f-%.1f encoberto_sat=%.3f-%.3f porta_faixa=%.1f.",
        settings.condition_adaptation_enabled ? "ativo" : "inativo",
        static_cast<double>(settings.condition_time_constant_seconds),
        static_cast<double>(settings.condition_log_seconds),
        static_cast<double>(settings.condition_daylight_median_low),
        static_cast<double>(settings.condition_daylight_median_high),
        static_cast<double>(settings.condition_overcast_saturation_low),
        static_cast<double>(settings.condition_overcast_saturation_high),
        static_cast<double>(settings.condition_minimum_dynamic_range));
    log_message(
        "Ancoras 0.19.0: sol=%.0fK/%.3f chuva=%.0fK/%.3f noite=%.0fK/%.3f "
        "(perfil fixo era %.0fK/%.3f em toda condicao).",
        static_cast<double>(settings.condition_sun_temperature),
        static_cast<double>(settings.condition_sun_tint),
        static_cast<double>(settings.condition_rain_temperature),
        static_cast<double>(settings.condition_rain_tint),
        static_cast<double>(settings.condition_night_temperature),
        static_cast<double>(settings.condition_night_tint),
        static_cast<double>(settings.temperature),
        static_cast<double>(settings.tint));
}

void log_profile(const CalibrationStack& stack, const Settings& settings) {
    if (!settings.photorealism_profile_enabled) {
        log_message(
            "Perfil photorealism 0.23.0: inativo; o grade vem das camadas "
            "medidas.");
        return;
    }
    const PhotorealismTonemap& tonemap = active_tonemap(stack.profile);
    log_message(
        "Perfil photorealism 0.23.0: ativo conjunto=%u temperatura=%.0f "
        "exposicao=%.3f contraste=%.3f saturacao=%.3f vibracao=%.3f "
        "sombras=%.3f altas_luzes=%.3f pretos=%.3f brancos=%.3f nitidez=%.1f "
        "bordas=%.1f ssao=x%.2f. Camadas medidas fora da composicao; cor sem "
        "adaptacao por condicao.",
        stack.profile.tonemap_set, tonemap.temperature, tonemap.exposure,
        tonemap.contrast, tonemap.saturation, tonemap.vibrance,
        tonemap.shadows, tonemap.highlights, tonemap.blacks, tonemap.whites,
        stack.profile.sharpness, stack.profile.sharpen_edges,
        stack.profile.ssao_intensity);
    if (uses_pending_controls(tonemap)) {
        log_message(
            "Perfil photorealism 0.23.0: o conjunto %u tem pre_exposure, "
            "pre_contrast, dynamic_contrast ou night_exposure fora do neutro, "
            "e esta versao ainda nao os aplica.",
            stack.profile.tonemap_set);
    }
}

}

void log_stack(const CalibrationStack& stack, const Settings& settings) {
    log_message(
        "Camadas cumulativas: base_0.1.2=%s visual_0.2.0=%s "
        "rain_overcast_0.3.0=%s%s.",
        stack.base.enabled ? "ativa" : "inativa",
        stack.visual_0_2.enabled ? "ativa" : "inativa",
        stack.rain_overcast_0_3.enabled ? "ativa" : "inativa",
        settings.photorealism_profile_enabled
            ? " (guardadas, fora da composicao pelo perfil)"
            : "");
    log_profile(stack, settings);
    log_effective_profile(settings);
    log_white_balance(settings);
    log_modules(settings);
}

}
