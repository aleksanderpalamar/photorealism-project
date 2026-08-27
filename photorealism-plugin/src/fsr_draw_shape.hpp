#pragma once

#include <cstdint>

// Predicados que definem o que a prova passiva aceita como um draw fullscreen
// de composicao final: viewport cobrindo toda a render target, scissor
// cobrindo toda a render target, e uma chamada de tres, quatro ou seis
// vertices sem instanciamento.
//
// Deliberadamente livre de Win32/D3D para poder ser testado no Linux. Os
// valores de topologia e de tipo de chamada sao replicados aqui como
// constantes e presos aos originais por static_assert em fsr_module.cpp, para
// que uma divergencia vire erro de compilacao e nao mudanca silenciosa de
// comportamento.
namespace photorealism::fsr {

constexpr float kViewportEpsilon = 0.01f;

constexpr std::uint32_t kTopologyTriangleList = 4;   // D3D11_..._TRIANGLELIST
constexpr std::uint32_t kTopologyTriangleStrip = 5;  // D3D11_..._TRIANGLESTRIP

// Espelham PHOTOREALISM_FSR_DRAW*.
constexpr std::uint32_t kDrawKindDraw = 1;
constexpr std::uint32_t kDrawKindDrawIndexed = 2;
constexpr std::uint32_t kDrawKindDrawInstanced = 3;
constexpr std::uint32_t kDrawKindDrawIndexedInstanced = 4;

// std::fabs nao e constexpr antes de C++23. A forma por subtracao dupla tem o
// mesmo resultado, inclusive com NaN, onde ambas as comparacoes sao falsas.
constexpr bool approximately_equal(float left, float right) {
    return (left - right <= kViewportEpsilon) &&
           (right - left <= kViewportEpsilon);
}

constexpr bool is_fullscreen_viewport(
    float top_left_x,
    float top_left_y,
    float width,
    float height,
    float min_depth,
    float max_depth,
    std::uint32_t target_width,
    std::uint32_t target_height) {
    return approximately_equal(top_left_x, 0.0f) &&
           approximately_equal(top_left_y, 0.0f) &&
           approximately_equal(width, static_cast<float>(target_width)) &&
           approximately_equal(height, static_cast<float>(target_height)) &&
           approximately_equal(min_depth, 0.0f) &&
           approximately_equal(max_depth, 1.0f);
}

constexpr bool is_fullscreen_scissor(
    std::int32_t left,
    std::int32_t top,
    std::int32_t right,
    std::int32_t bottom,
    std::uint32_t width,
    std::uint32_t height) {
    return left == 0 && top == 0 &&
           right == static_cast<std::int32_t>(width) &&
           bottom == static_cast<std::int32_t>(height);
}

// Um passe fullscreen desenha um triangulo, um quad ou dois triangulos, uma
// unica vez. Qualquer coisa fora disso e geometria de cena, HUD ou UI.
constexpr bool is_fullscreen_draw_shape(
    std::uint32_t kind,
    std::uint32_t primitive_count,
    std::uint32_t instance_count,
    std::uint32_t start_instance_location) {
    if (primitive_count != 3u && primitive_count != 4u &&
        primitive_count != 6u) {
        return false;
    }
    switch (kind) {
        case kDrawKindDraw:
        case kDrawKindDrawIndexed:
            return instance_count == 1u;
        case kDrawKindDrawInstanced:
        case kDrawKindDrawIndexedInstanced:
            return instance_count == 1u && start_instance_location == 0u;
        default:
            return false;
    }
}

constexpr bool is_fullscreen_topology(
    std::uint32_t topology, std::uint32_t count) {
    return (topology == kTopologyTriangleList &&
            (count == 3u || count == 6u)) ||
           (topology == kTopologyTriangleStrip && count == 4u);
}

}  // namespace photorealism::fsr
