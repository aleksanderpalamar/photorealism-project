#include "../src/fsr_rejected_draw_identity.hpp"

#include <cassert>
#include <string_view>

using photorealism::fsr::DrawProofRejectReason;
using photorealism::fsr::draw_proof_kind_name;
using photorealism::fsr::draw_proof_reject_reason_name;
using photorealism::fsr::kDrawKindDrawIndexed;
using photorealism::fsr::RejectedDrawIdentity;
using photorealism::fsr::rejected_draw_identity_matches;
using photorealism::fsr::viewport_to_milli;

// Um draw fullscreen rejeitado por scissor, do jeito que o Prism3D emite.
RejectedDrawIdentity fullscreen_scissor_reject() {
    RejectedDrawIdentity identity = {};
    identity.reason = DrawProofRejectReason::scissor;
    identity.kind = kDrawKindDrawIndexed;
    identity.topology = 4;
    identity.primitive_count = 3;
    identity.instance_count = 1;
    identity.source_identity = 0xAAAA0000ull;
    identity.source_width = 1920;
    identity.source_height = 1080;
    identity.source_format = 26;
    identity.rtv_identity = 0xBBBB0000ull;
    identity.rtv_width = 1920;
    identity.rtv_height = 1080;
    identity.rtv_format = 87;
    identity.rtv_count = 1;
    identity.pixel_shader = 0xCCCC0000ull;
    identity.depth_stencil_bound = false;
    identity.rasterizer_known = true;
    identity.viewport_known = true;
    identity.scissor_known = false;
    identity.scissor_enabled = false;
    identity.viewport_count = 1;
    identity.scissor_count = 0;
    identity.viewport_width_milli = viewport_to_milli(1920.0f);
    identity.viewport_height_milli = viewport_to_milli(1080.0f);
    return identity;
}

int main() {
    // O ponto da correcao: os campos que variam a cada chamada de um mesmo
    // passe logico nao existem na identidade, entao nao podem fragmenta-la.
    // Antes, cada offset de indice distinto criava uma assinatura nova e a
    // tabela de 64 slots saturava com draws equivalentes.
    {
        const RejectedDrawIdentity left = fullscreen_scissor_reject();
        const RejectedDrawIdentity right = fullscreen_scissor_reject();
        assert(rejected_draw_identity_matches(left, right));
    }

    // Diferencas estruturais reais precisam continuar separando assinaturas.
    {
        RejectedDrawIdentity other = fullscreen_scissor_reject();
        other.viewport_width_milli = viewport_to_milli(512.0f);
        assert(!rejected_draw_identity_matches(
            fullscreen_scissor_reject(), other));
    }
    {
        RejectedDrawIdentity other = fullscreen_scissor_reject();
        other.scissor_enabled = true;
        other.scissor_known = true;
        other.scissor_count = 1;
        other.scissor_right = 960;
        assert(!rejected_draw_identity_matches(
            fullscreen_scissor_reject(), other));
    }
    {
        RejectedDrawIdentity other = fullscreen_scissor_reject();
        other.pixel_shader = 0xDDDD0000ull;
        assert(!rejected_draw_identity_matches(
            fullscreen_scissor_reject(), other));
    }
    {
        RejectedDrawIdentity other = fullscreen_scissor_reject();
        other.rtv_format = 24;
        assert(!rejected_draw_identity_matches(
            fullscreen_scissor_reject(), other));
    }
    {
        RejectedDrawIdentity other = fullscreen_scissor_reject();
        other.depth_stencil_bound = true;
        assert(!rejected_draw_identity_matches(
            fullscreen_scissor_reject(), other));
    }
    // Motivos diferentes nunca podem compartilhar uma entrada: o relatorio
    // agrega por motivo antes de qualquer outra coisa.
    {
        RejectedDrawIdentity other = fullscreen_scissor_reject();
        other.reason = DrawProofRejectReason::viewport;
        assert(!rejected_draw_identity_matches(
            fullscreen_scissor_reject(), other));
    }

    // A identidade precisa distinguir "scissor desabilitado" de "scissor
    // habilitado cobrindo a tela inteira" -- os dois padroes que a revisao
    // passiva existe para separar.
    {
        RejectedDrawIdentity disabled = fullscreen_scissor_reject();
        RejectedDrawIdentity fullscreen_enabled = fullscreen_scissor_reject();
        fullscreen_enabled.scissor_enabled = true;
        fullscreen_enabled.scissor_known = true;
        fullscreen_enabled.scissor_count = 1;
        fullscreen_enabled.scissor_right = 1920;
        fullscreen_enabled.scissor_bottom = 1080;
        assert(!rejected_draw_identity_matches(disabled, fullscreen_enabled));
    }

    // Viewport em milesimos: 0.01 de diferenca precisa separar, e a conversao
    // nao pode truncar para o mesmo inteiro.
    {
        assert(viewport_to_milli(1920.0f) == 1920000);
        assert(viewport_to_milli(0.0f) == 0);
        assert(viewport_to_milli(1080.5f) == 1080500);
        assert(viewport_to_milli(-8.0f) == -8000);
        assert(viewport_to_milli(1920.01f) != viewport_to_milli(1920.0f));
    }

    // Nomes usados no log agregado.
    {
        assert(draw_proof_reject_reason_name(DrawProofRejectReason::scissor) ==
               std::string_view("scissor"));
        assert(draw_proof_reject_reason_name(
                   DrawProofRejectReason::raster_shadow) ==
               std::string_view("raster-shadow"));
        assert(draw_proof_kind_name(kDrawKindDrawIndexed) ==
               std::string_view("DrawIndexed"));
        assert(draw_proof_kind_name(99u) == std::string_view("unknown"));
    }

    return 0;
}
