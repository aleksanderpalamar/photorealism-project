#include "../src/rtgi_config.hpp"

#include <cassert>
#include <cstring>

int main() {
    using photorealism::rtgi::clamp_rtgi_settings;
    using photorealism::rtgi::default_rtgi_settings;
    using photorealism::rtgi::parse_rtgi_debug_mode;
    using photorealism::rtgi::rtgi_debug_mode_name;
    using photorealism::rtgi::rtgi_resolution;
    using photorealism::rtgi::next_rtgi_preview_debug;
    using photorealism::rtgi::rtgi_sample_distance;
    using photorealism::rtgi::rtgi_samples_within;
    using photorealism::rtgi::rtgi_step_ratio;
    using photorealism::rtgi::RtgiDebugMode;
    using photorealism::rtgi::RtgiSettings;

    // Os sete nomes do documento fazem ida e volta sem perder identidade.
    constexpr RtgiDebugMode modes[] = {
        RtgiDebugMode::normals,     RtgiDebugMode::rays,
        RtgiDebugMode::hit_distance, RtgiDebugMode::raw_gi,
        RtgiDebugMode::temporal_gi, RtgiDebugMode::confidence,
        RtgiDebugMode::final};
    for (RtgiDebugMode mode : modes) {
        assert(parse_rtgi_debug_mode(rtgi_debug_mode_name(mode)) == mode);
    }
    static_assert(
        parse_rtgi_debug_mode("hit_distance") == RtgiDebugMode::hit_distance);

    // Nome desconhecido, vazio ou nulo nunca pode pintar diagnostico na tela.
    assert(parse_rtgi_debug_mode("normal") == RtgiDebugMode::final);
    assert(parse_rtgi_debug_mode("") == RtgiDebugMode::final);
    assert(parse_rtgi_debug_mode(nullptr) == RtgiDebugMode::final);
    assert(parse_rtgi_debug_mode("normals ") == RtgiDebugMode::final);

    // O ciclo do Page Down fecha em cinco posicoes e volta ao inicio, sem
    // passar por temporal_gi (que so existe a partir da 0.12.3) nem por final
    // (que nao e diagnostico).
    static_assert(
        next_rtgi_preview_debug(RtgiDebugMode::normals) == RtgiDebugMode::rays);
    static_assert(
        next_rtgi_preview_debug(RtgiDebugMode::confidence) ==
        RtgiDebugMode::normals);
    RtgiDebugMode cycle = RtgiDebugMode::normals;
    for (int step = 0; step < 5; ++step) {
        cycle = next_rtgi_preview_debug(cycle);
        assert(cycle != RtgiDebugMode::temporal_gi);
        assert(cycle != RtgiDebugMode::final);
    }
    assert(cycle == RtgiDebugMode::normals);
    // Entrar no ciclo por um modo fora dele nao pode travar.
    assert(next_rtgi_preview_debug(RtgiDebugMode::final) ==
           RtgiDebugMode::normals);
    assert(next_rtgi_preview_debug(RtgiDebugMode::temporal_gi) ==
           RtgiDebugMode::normals);

    // A resolucao que o documento cita explicitamente.
    constexpr auto half = rtgi_resolution(1920, 1080, 2);
    static_assert(half.width == 960 && half.height == 540);
    constexpr auto quarter = rtgi_resolution(1920, 1080, 4);
    static_assert(quarter.width == 480 && quarter.height == 270);
    static_assert(rtgi_resolution(1920, 1080, 1).width == 1920);

    // Arredondamento para cima: nenhuma dimensao pode perder a ultima coluna.
    constexpr auto odd = rtgi_resolution(1921, 1081, 2);
    static_assert(odd.width == 961 && odd.height == 541);

    // Escala fora do intervalo e presa, nao propagada.
    static_assert(rtgi_resolution(1920, 1080, 0).width == 1920);
    static_assert(rtgi_resolution(1920, 1080, 99).width == 480);

    // Lado zero jamais sai daqui: seria uma falha de CreateTexture2D no meio
    // do frame.
    constexpr auto tiny = rtgi_resolution(3, 1, 4);
    static_assert(tiny.width == 1 && tiny.height == 1);
    static_assert(rtgi_resolution(0, 1080, 2).width == 0);

    // A marcha e geometrica: a razao e sempre maior que 1, senao ela nao
    // avanca. Com 0.10 a 15 m em 12 passos a razao fica perto de 1,518.
    assert(rtgi_step_ratio(0.10f, 15.0f, 12) > 1.51f);
    assert(rtgi_step_ratio(0.10f, 15.0f, 12) < 1.52f);

    // O ultimo passo pousa em range_max, e nao antes nem depois: e o que
    // garante que o alcance util declarado no cfg e o alcance real.
    assert(rtgi_sample_distance(0.10f, 15.0f, 12, 12) > 14.9f);
    assert(rtgi_sample_distance(0.10f, 15.0f, 12, 12) < 15.1f);

    // A primeira amostra tem que cair dentro da cabine. Com o passo fixo da
    // 0.12.1 ela caia em 1,71 m -- depois do para-brisa.
    assert(rtgi_sample_distance(0.10f, 15.0f, 12, 1) < 0.20f);

    // REGRESSAO 0.13.2: a cobertura da cabine. Banco a ~0,5 m, painel e GPS a
    // ~0,7 m, para-brisa a ~1 m. O passo fixo da 0.12.1 nao punha NENHUMA
    // amostra abaixo de 1,71 m, e por isso o RTGI nao alcancava o interior.
    assert(rtgi_samples_within(0.10f, 15.0f, 12, 1.5f) >= 4);
    assert(rtgi_samples_within(0.10f, 15.0f, 12, 1.0f) >= 3);

    // E o exterior nao pode ter sido sacrificado para isso: metade dos passos
    // continua alem de 1,5 m.
    assert(rtgi_samples_within(0.10f, 15.0f, 12, 1.5f) <= 8);

    // A marcha precisa avancar sempre: intervalo degenerado, invertido ou com
    // max_steps fora da faixa nao pode produzir razao <= 1, que travaria o
    // laco do shader na mesma distancia por 12 iteracoes.
    assert(rtgi_step_ratio(5.0f, 5.0f, 12) >= 1.0f);
    assert(rtgi_step_ratio(15.0f, 0.5f, 12) >= 1.0f);
    assert(rtgi_step_ratio(0.10f, 15.0f, 0) > 1.0f);
    assert(rtgi_step_ratio(0.10f, 15.0f, 9999) > 1.0f);
    assert(rtgi_step_ratio(0.10f, 15.0f, 9999) ==
           rtgi_step_ratio(0.10f, 15.0f, 24));

    // range_min abaixo do piso nao pode virar divisao por zero na razao.
    assert(rtgi_step_ratio(0.0f, 15.0f, 12) > 1.0f);
    assert(rtgi_sample_distance(0.0f, 15.0f, 12, 1) > 0.0f);

    // O padrao do documento sobrevive ao clamp sem ser alterado.
    constexpr RtgiSettings defaults = default_rtgi_settings();
    constexpr RtgiSettings clamped_defaults = clamp_rtgi_settings(defaults);
    static_assert(clamped_defaults.resolution_scale == 2);
    static_assert(clamped_defaults.ray_count == 4);
    static_assert(clamped_defaults.max_steps == 12);
    // O fallback interno tem que casar com o cfg: um cfg ausente nao
    // pode devolver o RTGI ao ponto cego da cabine.
    static_assert(clamped_defaults.range_min == 0.10f);
    assert(rtgi_samples_within(
               clamped_defaults.range_min,
               clamped_defaults.range_max,
               clamped_defaults.max_steps,
               1.5f) >= 4);
    static_assert(clamped_defaults.range_max == 15.0f);
    static_assert(clamped_defaults.gi_intensity == 0.15f);
    static_assert(clamped_defaults.history_weight == 0.90f);
    static_assert(clamped_defaults.hit_thickness == 0.5f);
    static_assert(clamped_defaults.normal_bias == 0.05f);
    static_assert(clamped_defaults.debug == RtgiDebugMode::final);
    static_assert(!clamped_defaults.enabled);

    // Um cfg absurdo nao pode descrever um dispatch invalido.
    RtgiSettings hostile = defaults;
    hostile.resolution_scale = 0;
    hostile.ray_count = 0;
    hostile.max_steps = 4096;
    hostile.range_min = -10.0f;
    hostile.range_max = -20.0f;
    hostile.gi_intensity = 50.0f;
    hostile.max_indirect_luma = 0.0f;
    hostile.sky_ambient = -1.0f;
    hostile.history_weight = 3.0f;
    hostile.depth_rejection = -0.5f;
    hostile.normal_rejection = 9.0f;
    hostile.color_rejection = -0.0001f;
    hostile.hit_thickness = 0.0f;
    hostile.normal_bias = -3.0f;
    const RtgiSettings safe = clamp_rtgi_settings(hostile);
    assert(safe.resolution_scale >= 1 && safe.resolution_scale <= 4);
    assert(safe.ray_count >= 1 && safe.ray_count <= 8);
    assert(safe.max_steps >= 1 && safe.max_steps <= 24);
    assert(safe.range_min > 0.0f);
    assert(safe.range_max > safe.range_min);
    assert(safe.gi_intensity >= 0.0f && safe.gi_intensity <= 1.0f);
    assert(safe.max_indirect_luma > 0.0f);
    assert(safe.sky_ambient >= 0.0f);
    assert(safe.history_weight >= 0.0f && safe.history_weight <= 1.0f);
    assert(safe.depth_rejection >= 0.0f && safe.depth_rejection <= 1.0f);
    assert(safe.normal_rejection >= 0.0f && safe.normal_rejection <= 1.0f);
    assert(safe.color_rejection >= 0.0f && safe.color_rejection <= 1.0f);
    // Espessura zero aceitaria qualquer coisa atras da geometria como acerto.
    assert(safe.hit_thickness > 0.0f);
    assert(safe.normal_bias >= 0.0f);

    // Intervalo invertido vira intervalo valido, nunca vazio.
    RtgiSettings inverted = defaults;
    inverted.range_min = 12.0f;
    inverted.range_max = 3.0f;
    const RtgiSettings ordered = clamp_rtgi_settings(inverted);
    assert(ordered.range_min == 12.0f);
    assert(ordered.range_max > ordered.range_min);

    // NaN cai no minimo seguro em vez de contaminar o dispatch.
    RtgiSettings poisoned = defaults;
    const float zero = 0.0f;
    const float nan_value = zero / zero;
    poisoned.gi_intensity = nan_value;
    poisoned.history_weight = nan_value;
    poisoned.range_min = nan_value;
    poisoned.hit_thickness = nan_value;
    const RtgiSettings healthy = clamp_rtgi_settings(poisoned);
    assert(healthy.gi_intensity == 0.0f);
    assert(healthy.history_weight == 0.0f);
    assert(healthy.range_min > 0.0f);
    assert(healthy.range_max > healthy.range_min);
    assert(healthy.hit_thickness > 0.0f);

    // Os presets Low/Medium/High do documento cabem nos limites.
    RtgiSettings low = defaults;
    low.resolution_scale = 4;
    low.ray_count = 2;
    low.max_steps = 8;
    assert(clamp_rtgi_settings(low).ray_count == 2);
    assert(clamp_rtgi_settings(low).max_steps == 8);
    RtgiSettings high = defaults;
    high.ray_count = 8;
    high.max_steps = 24;
    assert(clamp_rtgi_settings(high).ray_count == 8);
    assert(clamp_rtgi_settings(high).max_steps == 24);

    assert(std::strcmp(rtgi_debug_mode_name(RtgiDebugMode::raw_gi),
                       "raw_gi") == 0);
    return 0;
}
