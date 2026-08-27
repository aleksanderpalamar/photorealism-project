#include "../src/fsr_draw_shape.hpp"

#include <cassert>

using photorealism::fsr::approximately_equal;
using photorealism::fsr::is_fullscreen_draw_shape;
using photorealism::fsr::is_fullscreen_scissor;
using photorealism::fsr::is_fullscreen_topology;
using photorealism::fsr::is_fullscreen_viewport;
using photorealism::fsr::kDrawKindDraw;
using photorealism::fsr::kDrawKindDrawIndexed;
using photorealism::fsr::kDrawKindDrawIndexedInstanced;
using photorealism::fsr::kDrawKindDrawInstanced;
using photorealism::fsr::kTopologyTriangleList;
using photorealism::fsr::kTopologyTriangleStrip;

int main() {
    // Tolerancia de 0.01, nos dois sentidos.
    static_assert(approximately_equal(1920.0f, 1920.0f));
    static_assert(approximately_equal(1920.0f, 1920.009f));
    static_assert(approximately_equal(1920.009f, 1920.0f));
    static_assert(!approximately_equal(1920.0f, 1920.5f));
    static_assert(!approximately_equal(1920.5f, 1920.0f));

    // Viewport fullscreen canonico do ETS2 em 1080p.
    static_assert(is_fullscreen_viewport(
        0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 1.0f, 1920, 1080));
    // Viewport parcial: espelho, reflexo, render target auxiliar.
    static_assert(!is_fullscreen_viewport(
        0.0f, 0.0f, 512.0f, 512.0f, 0.0f, 1.0f, 1920, 1080));
    // Origem deslocada nao e fullscreen mesmo com as dimensoes certas.
    static_assert(!is_fullscreen_viewport(
        8.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 1.0f, 1920, 1080));
    // Faixa de profundidade fora de [0,1] nao e um passe de composicao.
    static_assert(!is_fullscreen_viewport(
        0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.5f, 1920, 1080));

    static_assert(is_fullscreen_scissor(0, 0, 1920, 1080, 1920, 1080));
    static_assert(!is_fullscreen_scissor(0, 0, 960, 1080, 1920, 1080));
    static_assert(!is_fullscreen_scissor(8, 0, 1920, 1080, 1920, 1080));

    // Triangulo unico, quad e dois triangulos, sem instanciamento.
    static_assert(is_fullscreen_draw_shape(kDrawKindDrawIndexed, 3, 1, 0));
    static_assert(is_fullscreen_draw_shape(kDrawKindDraw, 4, 1, 0));
    static_assert(is_fullscreen_draw_shape(kDrawKindDrawIndexed, 6, 1, 0));
    // Contagem de vertices de geometria de cena.
    static_assert(!is_fullscreen_draw_shape(kDrawKindDrawIndexed, 5, 1, 0));
    static_assert(!is_fullscreen_draw_shape(kDrawKindDrawIndexed, 36, 1, 0));
    // Instanciado de verdade nao e passe fullscreen.
    static_assert(!is_fullscreen_draw_shape(kDrawKindDrawInstanced, 3, 8, 0));
    // Instanciado com uma instancia passa, mas so a partir da instancia zero.
    static_assert(is_fullscreen_draw_shape(kDrawKindDrawInstanced, 3, 1, 0));
    static_assert(
        !is_fullscreen_draw_shape(kDrawKindDrawIndexedInstanced, 3, 1, 4));
    // Tipo de chamada desconhecido nunca passa.
    static_assert(!is_fullscreen_draw_shape(0, 3, 1, 0));
    static_assert(!is_fullscreen_draw_shape(99, 3, 1, 0));

    // A topologia tem de bater com a contagem: quad so como strip.
    static_assert(is_fullscreen_topology(kTopologyTriangleList, 3));
    static_assert(is_fullscreen_topology(kTopologyTriangleList, 6));
    static_assert(!is_fullscreen_topology(kTopologyTriangleList, 4));
    static_assert(is_fullscreen_topology(kTopologyTriangleStrip, 4));
    static_assert(!is_fullscreen_topology(kTopologyTriangleStrip, 3));
    // Lista de pontos ou de linhas nunca compoe a tela.
    static_assert(!is_fullscreen_topology(1, 3));
    static_assert(!is_fullscreen_topology(2, 3));

    return 0;
}
