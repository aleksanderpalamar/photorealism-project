#pragma once

#include <cstdint>

// Chave de agrupamento dos draws rejeitados pela prova passiva do draw final.
//
// O objetivo do diagnostico e transformar milhares de rejeicoes repetidas em
// poucas entradas estruturais. Isso so funciona se draws estruturalmente
// equivalentes cairem na mesma assinatura, entao tudo que varia entre chamadas
// de um mesmo passe logico -- offsets de indice, vertice base, instancia
// inicial e o ponteiro do depth-stencil view -- fica deliberadamente fora
// daqui e e guardado a parte, como amostra da primeira ocorrencia.
//
// Deliberadamente livre de Win32/D3D para poder ser testado no Linux.
namespace photorealism::fsr {

enum class DrawProofRejectReason : std::uint32_t {
    missing_rtv,
    multiple_rtvs,
    depth_bound,
    wrong_backbuffer,
    source_mismatch,
    source_ineligible,
    viewport,
    scissor,
    pixel_shader,
    topology,
    call_shape,
    raster_shadow,
};

// Comparada campo a campo, nunca com memcmp: o tipo tem padding e a
// inicializacao de agregado nao garante que ele venha zerado, o que faria
// draws identicos parecerem distintos conforme o lixo da pilha.
struct RejectedDrawIdentity {
    DrawProofRejectReason reason = DrawProofRejectReason::missing_rtv;

    std::uint32_t kind = 0;
    std::uint32_t topology = 0;
    std::uint32_t primitive_count = 0;
    std::uint32_t instance_count = 0;

    std::uint64_t source_identity = 0;
    std::uint32_t source_width = 0;
    std::uint32_t source_height = 0;
    std::uint32_t source_format = 0;

    std::uint64_t rtv_identity = 0;
    std::uint32_t rtv_width = 0;
    std::uint32_t rtv_height = 0;
    std::uint32_t rtv_format = 0;
    std::uint32_t rtv_count = 0;

    std::uint64_t pixel_shader = 0;

    // Presenca, nao o ponteiro: o objeto muda entre passes sem que o draw
    // mude de natureza, e "tinha depth" e a unica informacao que a analise usa.
    bool depth_stencil_bound = false;

    bool rasterizer_known = false;
    bool viewport_known = false;
    bool scissor_known = false;
    bool scissor_enabled = false;

    std::uint32_t viewport_count = 0;
    std::uint32_t scissor_count = 0;

    // Slot 0 apenas. Viewport em milesimos para que a comparacao seja exata
    // sem depender de igualdade de ponto flutuante.
    std::int32_t viewport_x_milli = 0;
    std::int32_t viewport_y_milli = 0;
    std::int32_t viewport_width_milli = 0;
    std::int32_t viewport_height_milli = 0;

    std::int32_t scissor_left = 0;
    std::int32_t scissor_top = 0;
    std::int32_t scissor_right = 0;
    std::int32_t scissor_bottom = 0;
};

// Registrada na primeira ocorrencia da identidade e nunca comparada. Sao os
// "draw arguments" que o diagnostico precisa reportar sem deixa-los fragmentar
// a agregacao.
struct RejectedDrawSample {
    std::uint32_t start_location = 0;
    std::int32_t base_vertex_location = 0;
    std::uint32_t start_instance_location = 0;
    std::uint64_t depth_stencil_view = 0;
};

constexpr std::int32_t viewport_to_milli(float value) {
    return static_cast<std::int32_t>(
        value < 0.0f ? value * 1000.0f - 0.5f : value * 1000.0f + 0.5f);
}

constexpr bool rejected_draw_identity_matches(
    const RejectedDrawIdentity& left, const RejectedDrawIdentity& right) {
    return left.reason == right.reason && left.kind == right.kind &&
           left.topology == right.topology &&
           left.primitive_count == right.primitive_count &&
           left.instance_count == right.instance_count &&
           left.source_identity == right.source_identity &&
           left.source_width == right.source_width &&
           left.source_height == right.source_height &&
           left.source_format == right.source_format &&
           left.rtv_identity == right.rtv_identity &&
           left.rtv_width == right.rtv_width &&
           left.rtv_height == right.rtv_height &&
           left.rtv_format == right.rtv_format &&
           left.rtv_count == right.rtv_count &&
           left.pixel_shader == right.pixel_shader &&
           left.depth_stencil_bound == right.depth_stencil_bound &&
           left.rasterizer_known == right.rasterizer_known &&
           left.viewport_known == right.viewport_known &&
           left.scissor_known == right.scissor_known &&
           left.scissor_enabled == right.scissor_enabled &&
           left.viewport_count == right.viewport_count &&
           left.scissor_count == right.scissor_count &&
           left.viewport_x_milli == right.viewport_x_milli &&
           left.viewport_y_milli == right.viewport_y_milli &&
           left.viewport_width_milli == right.viewport_width_milli &&
           left.viewport_height_milli == right.viewport_height_milli &&
           left.scissor_left == right.scissor_left &&
           left.scissor_top == right.scissor_top &&
           left.scissor_right == right.scissor_right &&
           left.scissor_bottom == right.scissor_bottom;
}

constexpr const char* draw_proof_reject_reason_name(
    DrawProofRejectReason reason) {
    switch (reason) {
        case DrawProofRejectReason::missing_rtv:
            return "missing-rtv";
        case DrawProofRejectReason::multiple_rtvs:
            return "multiple-rtvs";
        case DrawProofRejectReason::depth_bound:
            return "depth-bound";
        case DrawProofRejectReason::wrong_backbuffer:
            return "wrong-backbuffer";
        case DrawProofRejectReason::source_mismatch:
            return "source-mismatch";
        case DrawProofRejectReason::source_ineligible:
            return "source-ineligible";
        case DrawProofRejectReason::viewport:
            return "viewport";
        case DrawProofRejectReason::scissor:
            return "scissor";
        case DrawProofRejectReason::pixel_shader:
            return "pixel-shader";
        case DrawProofRejectReason::topology:
            return "topology";
        case DrawProofRejectReason::call_shape:
            return "call-shape";
        case DrawProofRejectReason::raster_shadow:
            return "raster-shadow";
    }
    return "unknown";
}

// Espelha PHOTOREALISM_FSR_DRAW*; fsr_module.cpp prende esses valores aos da
// ABI com static_assert, para que uma divergencia vire erro de compilacao.
constexpr std::uint32_t kDrawKindDraw = 1;
constexpr std::uint32_t kDrawKindDrawIndexed = 2;
constexpr std::uint32_t kDrawKindDrawInstanced = 3;
constexpr std::uint32_t kDrawKindDrawIndexedInstanced = 4;

constexpr const char* draw_proof_kind_name(std::uint32_t kind) {
    switch (kind) {
        case kDrawKindDraw:
            return "Draw";
        case kDrawKindDrawIndexed:
            return "DrawIndexed";
        case kDrawKindDrawInstanced:
            return "DrawInstanced";
        case kDrawKindDrawIndexedInstanced:
            return "DrawIndexedInstanced";
        default:
            return "unknown";
    }
}

}  // namespace photorealism::fsr
