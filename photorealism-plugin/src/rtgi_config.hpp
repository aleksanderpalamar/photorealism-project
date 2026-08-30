#pragma once

#include <cmath>
#include <cstdint>

// Regras puras do Screen-Space Ray-Traced Global Illumination.
//
// Este header nao conhece Win32, D3D nem o arquivo de configuracao: recebe
// escalares e devolve escalares, para que as decisoes que definem o custo e a
// validade de um dispatch possam ser testadas no Linux, sem jogo e sem GPU.
namespace photorealism::rtgi {

// Os sete modos que o documento da tecnica define para `rtgi_debug`.
enum class RtgiDebugMode : std::uint32_t {
    normals = 0,
    rays = 1,
    hit_distance = 2,
    raw_gi = 3,
    temporal_gi = 4,
    confidence = 5,
    final = 6,
};

constexpr std::uint32_t kMinimumResolutionScale = 1;
constexpr std::uint32_t kMaximumResolutionScale = 4;
constexpr std::uint32_t kMinimumRayCount = 1;
constexpr std::uint32_t kMaximumRayCount = 8;
constexpr std::uint32_t kMinimumMaxSteps = 1;
constexpr std::uint32_t kMaximumMaxSteps = 24;

// O alcance curto/medio e deliberado: o ganho visual no ETS2 esta em cabine,
// asfalto, paredes e vegetacao proxima. Rastrear ate uma montanha a 2 km
// custaria passos sem entregar bounce visivel.
constexpr float kMinimumRange = 0.05f;
constexpr float kMaximumRange = 200.0f;
constexpr float kMinimumRangeSeparation = 0.05f;

// GI exagerada produz o visual de mod: paredes neon e cabine radioativa. O
// teto existe para que um cfg editado a mao nao chegue la sem querer.
constexpr float kMaximumGiIntensity = 1.0f;
constexpr float kMinimumMaxIndirectLuma = 0.01f;
constexpr float kMaximumMaxIndirectLuma = 64.0f;
constexpr float kMaximumSkyAmbient = 16.0f;

// Screen-space nao conhece a profundidade real do que o raio atinge: so sabe o
// depth da superficie visivel naquele texel. Sem um limite de espessura,
// qualquer coisa atras da geometria conta como acerto e a luz vaza por tras
// das paredes.
constexpr float kMinimumHitThickness = 0.01f;
constexpr float kMaximumHitThickness = 10.0f;

// Deslocamento da origem ao longo da normal, para o raio nao acertar a
// propria superficie no primeiro passo.
constexpr float kMinimumNormalBias = 0.0f;
constexpr float kMaximumNormalBias = 1.0f;

constexpr std::uint32_t clamp_unsigned(
    std::uint32_t value, std::uint32_t minimum, std::uint32_t maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

constexpr float clamp_float(float value, float minimum, float maximum) {
    // NaN falha as duas comparacoes e cai no minimo, que e o valor seguro.
    if (!(value > minimum)) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

constexpr const char* rtgi_debug_mode_name(RtgiDebugMode mode) {
    switch (mode) {
        case RtgiDebugMode::normals:
            return "normals";
        case RtgiDebugMode::rays:
            return "rays";
        case RtgiDebugMode::hit_distance:
            return "hit_distance";
        case RtgiDebugMode::raw_gi:
            return "raw_gi";
        case RtgiDebugMode::temporal_gi:
            return "temporal_gi";
        case RtgiDebugMode::confidence:
            return "confidence";
        case RtgiDebugMode::final:
            return "final";
    }
    return "final";
}

constexpr bool text_matches(const char* left, const char* right) {
    if (left == nullptr || right == nullptr) {
        return false;
    }
    while (*left != '\0' && *right != '\0' && *left == *right) {
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

// Nome desconhecido cai em `final`, que e o modo que nao desenha diagnostico
// nenhum: um cfg com erro de digitacao nunca deve pintar a tela.
constexpr RtgiDebugMode parse_rtgi_debug_mode(const char* name) {
    if (text_matches(name, "normals")) {
        return RtgiDebugMode::normals;
    }
    if (text_matches(name, "rays")) {
        return RtgiDebugMode::rays;
    }
    if (text_matches(name, "hit_distance")) {
        return RtgiDebugMode::hit_distance;
    }
    if (text_matches(name, "raw_gi")) {
        return RtgiDebugMode::raw_gi;
    }
    if (text_matches(name, "temporal_gi")) {
        return RtgiDebugMode::temporal_gi;
    }
    if (text_matches(name, "confidence")) {
        return RtgiDebugMode::confidence;
    }
    return RtgiDebugMode::final;
}

// Ciclo das debug views no preview, na ordem em que sao uteis para depurar a
// marcha: primeiro a geometria, depois os raios, depois o resultado.
// temporal_gi fica de fora ate a 0.12.3 existir, e final nao e diagnostico.
constexpr RtgiDebugMode next_rtgi_preview_debug(RtgiDebugMode mode) {
    switch (mode) {
        case RtgiDebugMode::normals:
            return RtgiDebugMode::rays;
        case RtgiDebugMode::rays:
            return RtgiDebugMode::hit_distance;
        case RtgiDebugMode::hit_distance:
            return RtgiDebugMode::raw_gi;
        case RtgiDebugMode::raw_gi:
            return RtgiDebugMode::confidence;
        case RtgiDebugMode::confidence:
        case RtgiDebugMode::temporal_gi:
        case RtgiDebugMode::final:
            return RtgiDebugMode::normals;
    }
    return RtgiDebugMode::normals;
}

struct RtgiDimensions {
    std::uint32_t width;
    std::uint32_t height;
};

// Divisao com arredondamento para cima e piso de 1: nenhuma resolucao de
// entrada pode produzir uma textura de lado zero, que falharia em
// CreateTexture2D no meio do frame.
constexpr RtgiDimensions rtgi_resolution(
    std::uint32_t width, std::uint32_t height, std::uint32_t scale) {
    if (width == 0 || height == 0) {
        return {0, 0};
    }
    std::uint32_t divisor = scale;
    if (divisor < kMinimumResolutionScale) {
        divisor = kMinimumResolutionScale;
    }
    if (divisor > kMaximumResolutionScale) {
        divisor = kMaximumResolutionScale;
    }
    const std::uint32_t scaled_width = (width + divisor - 1) / divisor;
    const std::uint32_t scaled_height = (height + divisor - 1) / divisor;
    return {
        scaled_width == 0 ? 1u : scaled_width,
        scaled_height == 0 ? 1u : scaled_height};
}

// Progressao geometrica em view-space, e nao passo fixo.
//
// O passo fixo da 0.12.1 era cego para a cabine por construcao: com
// range 0.5-15.0 em 12 passos o passo vale 1,21 m, e como `travelled` comeca em
// range_min e o laco soma antes de amostrar, nada era amostrado entre 0,5 e
// 1,71 m -- justamente onde vivem banco (~0,5 m), painel e GPS (~0,7 m) e
// para-brisa (~1 m). Um raio saindo do painel pulava a cabine inteira.
//
// A razao geometrica distribui os mesmos passos em escala logaritmica: fino
// perto, grosso longe. Com 0.10-15.0 em 12 passos, seis amostras caem dentro da
// cabine e o alcance externo continua chegando a 15 m.
inline float rtgi_step_ratio(
    float range_min, float range_max, std::uint32_t max_steps) {
    const std::uint32_t steps = clamp_unsigned(
        max_steps, kMinimumMaxSteps, kMaximumMaxSteps);
    const float near_bound =
        range_min > kMinimumRange ? range_min : kMinimumRange;
    if (!(range_max > near_bound)) {
        return 1.0f;
    }
    return std::pow(
        range_max / near_bound, 1.0f / static_cast<float>(steps));
}

// Distancia da k-esima amostra, com k de 1 a max_steps. k=0 devolve a origem
// da marcha, que nunca e amostrada.
inline float rtgi_sample_distance(
    float range_min,
    float range_max,
    std::uint32_t max_steps,
    std::uint32_t index) {
    const float near_bound =
        range_min > kMinimumRange ? range_min : kMinimumRange;
    const float ratio = rtgi_step_ratio(range_min, range_max, max_steps);
    float distance = near_bound;
    for (std::uint32_t step = 0; step < index; ++step) {
        distance *= ratio;
    }
    return distance;
}

// Quantas amostras caem abaixo de um limite. Existe para que a cobertura da
// cabine seja uma invariante testada, e nao uma aritmetica refeita na mao a
// cada mudanca de parametro -- foi exatamente essa conta que ninguem fez ate a
// 0.13.2, e o RTGI passou duas versoes sem alcancar o interior.
inline std::uint32_t rtgi_samples_within(
    float range_min,
    float range_max,
    std::uint32_t max_steps,
    float limit) {
    const std::uint32_t steps = clamp_unsigned(
        max_steps, kMinimumMaxSteps, kMaximumMaxSteps);
    std::uint32_t found = 0;
    for (std::uint32_t index = 1; index <= steps; ++index) {
        if (rtgi_sample_distance(range_min, range_max, max_steps, index) <=
            limit) {
            ++found;
        }
    }
    return found;
}

struct RtgiSettings {
    bool enabled;
    std::uint32_t resolution_scale;
    std::uint32_t ray_count;
    std::uint32_t max_steps;
    float range_min;
    float range_max;
    float gi_intensity;
    float max_indirect_luma;
    float sky_ambient;
    float hit_thickness;
    float normal_bias;
    float history_weight;
    float depth_rejection;
    float normal_rejection;
    float color_rejection;
    RtgiDebugMode debug;
};

// Os limites vem do documento da tecnica. Um cfg editado a mao nunca deve
// conseguir descrever um dispatch invalido: intervalo invertido, contagem de
// raios zerada ou peso de historico fora de [0, 1].
constexpr RtgiSettings clamp_rtgi_settings(RtgiSettings settings) {
    settings.resolution_scale = clamp_unsigned(
        settings.resolution_scale,
        kMinimumResolutionScale,
        kMaximumResolutionScale);
    settings.ray_count = clamp_unsigned(
        settings.ray_count, kMinimumRayCount, kMaximumRayCount);
    settings.max_steps = clamp_unsigned(
        settings.max_steps, kMinimumMaxSteps, kMaximumMaxSteps);

    settings.range_min = clamp_float(
        settings.range_min, kMinimumRange, kMaximumRange);
    settings.range_max = clamp_float(
        settings.range_max, kMinimumRange, kMaximumRange);
    if (settings.range_max < settings.range_min + kMinimumRangeSeparation) {
        settings.range_max = settings.range_min + kMinimumRangeSeparation;
    }

    settings.gi_intensity = clamp_float(
        settings.gi_intensity, 0.0f, kMaximumGiIntensity);
    settings.max_indirect_luma = clamp_float(
        settings.max_indirect_luma,
        kMinimumMaxIndirectLuma,
        kMaximumMaxIndirectLuma);
    settings.sky_ambient = clamp_float(
        settings.sky_ambient, 0.0f, kMaximumSkyAmbient);
    settings.hit_thickness = clamp_float(
        settings.hit_thickness, kMinimumHitThickness, kMaximumHitThickness);
    settings.normal_bias = clamp_float(
        settings.normal_bias, kMinimumNormalBias, kMaximumNormalBias);

    settings.history_weight = clamp_float(settings.history_weight, 0.0f, 1.0f);
    settings.depth_rejection =
        clamp_float(settings.depth_rejection, 0.0f, 1.0f);
    settings.normal_rejection =
        clamp_float(settings.normal_rejection, 0.0f, 1.0f);
    settings.color_rejection =
        clamp_float(settings.color_rejection, 0.0f, 1.0f);
    return settings;
}

// Os valores que o documento fixa para a primeira entrega.
//
// sky_ambient deixou de ser 0.0 na 0.13.2.1. Zero fazia o preto ser a resposta
// do shader para todo raio que escapava sem acertar nada, e dentro da cabine
// escapam todos: o hemisferio de uma superficie virada para a camera e o ar
// entre ela e o olho. O painel ficava preto chapado e sem nem o granulado que
// denunciaria o problema.
//
// 0.25 nao e a radiancia de um ceu -- e a de uma direcao DESCONHECIDA, e em
// jogo a maioria delas esta parcialmente ocluida (cabine, tunel, viaduto,
// vao entre predios). Um quarto de um ceu encoberto tipico e o lado
// conservador dessa conta; se o interior sair escuro demais, subir este valor
// e edicao de cfg, sem recompilar.
constexpr RtgiSettings default_rtgi_settings() {
    return {
        false,
        2,
        4,
        12,
        0.10f,
        15.0f,
        0.15f,
        4.0f,
        0.25f,
        0.5f,
        0.05f,
        0.90f,
        0.015f,
        0.85f,
        0.15f,
        RtgiDebugMode::final,
    };
}

}  // namespace photorealism::rtgi
