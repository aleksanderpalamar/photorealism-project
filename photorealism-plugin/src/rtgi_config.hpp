#pragma once

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
    float history_weight;
    float depth_rejection;
    float normal_rejection;
    float color_rejection;
    RtgiDebugMode debug;
};

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
constexpr RtgiSettings default_rtgi_settings() {
    return {
        false,
        2,
        4,
        12,
        0.5f,
        15.0f,
        0.15f,
        4.0f,
        0.0f,
        0.90f,
        0.015f,
        0.85f,
        0.15f,
        RtgiDebugMode::final,
    };
}

}  // namespace photorealism::rtgi
