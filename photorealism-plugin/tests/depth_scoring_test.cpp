#include "../src/depth_scoring.hpp"

#include <cassert>
#include <string>

int main() {
    using photorealism::depth_scoring::is_confident_candidate;
    using photorealism::depth_scoring::is_scene_candidate;
    using photorealism::depth_scoring::resource_score;

    constexpr unsigned backbuffer_width = 1920;
    constexpr unsigned backbuffer_height = 1080;

    const auto ets2_interface = resource_score(
        1920, 1080, 7499, backbuffer_width, backbuffer_height);
    const auto ets2_world = resource_score(
        1920, 2160, 32818, backbuffer_width, backbuffer_height);
    const auto ets2_shadow = resource_score(
        4096, 4096, 7236, backbuffer_width, backbuffer_height);
    assert(ets2_world > ets2_interface);
    assert(ets2_world > ets2_shadow);
    assert(is_scene_candidate(
        1920, 2160, 32818, 1, backbuffer_width, backbuffer_height, 30000));

    // Ate a 0.13.0 esta linha era `!is_scene_candidate`: um alvo do tamanho
    // exato da tela era tratado como interface, nao como cena. A separacao era
    // so a taxa de binds, e o depth de camera real do ETS2 sem supersampling
    // faz 291/s -- abaixo da linha. A elegibilidade nao precisa fazer essa
    // separacao, porque o score ja faz: quando o mundo supersampleado existe,
    // ele vence a selecao. A assercao abaixo e a invariante que importa.
    assert(is_scene_candidate(
        1920, 1080, 7499, 1, backbuffer_width, backbuffer_height, 30000));
    assert(ets2_world > ets2_interface);

    const auto ats_interface = resource_score(
        1920, 1080, 140, backbuffer_width, backbuffer_height);
    const auto ats_world = resource_score(
        2400, 1350, 30000, backbuffer_width, backbuffer_height);
    assert(ats_world > ats_interface);
    assert(!is_confident_candidate(
        1920, 1080, 140, 1, backbuffer_width, backbuffer_height));
    assert(is_confident_candidate(
        2400, 1350, 30000, 1, backbuffer_width, backbuffer_height));
    assert(is_scene_candidate(
        2400, 1350, 30000, 1, backbuffer_width, backbuffer_height, 30000));

    assert(is_scene_candidate(
        1920, 1080, 15000, 1, backbuffer_width, backbuffer_height, 30000));
    assert(is_scene_candidate(
        1920, 2160, 1000, 1, backbuffer_width, backbuffer_height, 3000));

    assert(!is_confident_candidate(
        1920, 2160, 32818, 4, backbuffer_width, backbuffer_height));
    assert(!is_scene_candidate(
        1920, 2160, 32818, 4, backbuffer_width, backbuffer_height, 30000));
    // Numeros reais do log de 28/08, sessao em que o RTGI nunca rodou.
    //
    // O depth de camera do ETS2 sem supersampling (r_scale_x=1, r_scale_y=1)
    // tem exatamente a area da tela. A regra dos 110% foi escrita para o caso
    // supersampleado e o excluia por construcao; a valvula de 400 binds/s
    // tambem nao alcancava, porque ele faz 291/s. Sobrava um shadow map
    // 2048x2048, quadrado, com 202% da area -- e ele vencia.
    assert(is_scene_candidate(
        1920, 1080, 8730, 1, backbuffer_width, backbuffer_height, 30000));
    assert(!is_scene_candidate(
        2048, 2048, 1043, 1, backbuffer_width, backbuffer_height, 30000));
    assert(!is_scene_candidate(
        4096, 4096, 343, 1, backbuffer_width, backbuffer_height, 30000));

    // Proporcao e veto duro: nem atividade sustentada salva um alvo que nao
    // tem a forma da tela.
    assert(!is_scene_candidate(
        2048, 2048, 900000, 1, backbuffer_width, backbuffer_height, 30000));
    assert(!is_scene_candidate(
        512, 1024, 900000, 1, backbuffer_width, backbuffer_height, 30000));

    // O caso supersampleado que a regra original protegia continua valendo.
    assert(is_scene_candidate(
        1920, 2160, 32818, 1, backbuffer_width, backbuffer_height, 30000));

    // Meia resolucao nao e o depth principal, mesmo com proporcao correta.
    assert(!is_scene_candidate(
        960, 540, 8730, 1, backbuffer_width, backbuffer_height, 30000));

    // O diagnostico precisa apontar o motivo exato, nao so "rejeitado".
    using photorealism::depth_scoring::depth_candidate_rejection;
    using photorealism::depth_scoring::depth_rejection_name;
    using photorealism::depth_scoring::DepthRejection;
    assert(depth_candidate_rejection(
               1920, 1080, 8730, 1, backbuffer_width, backbuffer_height,
               30000) == DepthRejection::none);
    assert(depth_candidate_rejection(
               2048, 2048, 1043, 1, backbuffer_width, backbuffer_height,
               30000) == DepthRejection::shape);
    assert(depth_candidate_rejection(
               1920, 1080, 140, 1, backbuffer_width, backbuffer_height,
               30000) == DepthRejection::bindings);
    assert(depth_candidate_rejection(
               1920, 1080, 8730, 4, backbuffer_width, backbuffer_height,
               30000) == DepthRejection::samples);
    // Meia resolucao e um quarto da area: cai no piso antes de chegar na
    // regra de area util.
    assert(depth_candidate_rejection(
               960, 540, 8730, 1, backbuffer_width, backbuffer_height,
               30000) == DepthRejection::too_small);
    // 1600x900 passa do piso e tem a forma certa, mas nao alcanca nem os 95%
    // de area nem os 400 binds/s.
    assert(depth_candidate_rejection(
               1600, 900, 8730, 1, backbuffer_width, backbuffer_height,
               30000) == DepthRejection::area_and_activity);
    assert(depth_candidate_rejection(
               0, 1080, 8730, 1, backbuffer_width, backbuffer_height,
               30000) == DepthRejection::invalid);

    // O motivo "aceito" tem que coincidir com is_scene_candidate, sempre.
    struct Case {
        unsigned width;
        unsigned height;
        unsigned long long bindings;
    };
    const Case cases[] = {
        {1920, 1080, 8730}, {2048, 2048, 1043}, {1920, 2160, 32818},
        {4096, 4096, 343},  {2400, 1350, 30000}, {960, 540, 8730},
        {1600, 900, 8730},
        {512, 1024, 900000}};
    for (const Case& item : cases) {
        const bool accepted = is_scene_candidate(
            item.width, item.height, item.bindings, 1, backbuffer_width,
            backbuffer_height, 30000);
        const bool reported =
            depth_candidate_rejection(
                item.width, item.height, item.bindings, 1, backbuffer_width,
                backbuffer_height, 30000) == DepthRejection::none;
        assert(accepted == reported);
    }
    assert(depth_rejection_name(DepthRejection::shape) ==
           std::string("forma-incompativel"));

    return 0;
}
