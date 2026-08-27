#pragma once

#include <d3d11.h>

#include <array>
#include <cstdint>

// Shadow do estado de rasterizacao por ID3D11DeviceContext.
//
// A prova passiva do draw final precisa saber viewport, scissor e ScissorEnable
// no momento de cada draw. Consultar RSGetViewports/RSGetScissorRects/RSGetState
// a cada Draw seria caro: o numero de draws por frame chega aos milhares.
// Em vez disso, os hooks de RSSetState/RSSetViewports/RSSetScissorRects
// alimentam este shadow, e a prova apenas le o ultimo estado observado.
//
// Nada aqui retem interfaces COM nem altera o pipeline: os valores sao copiados
// e os objetos consultados sao liberados na mesma chamada.

constexpr std::size_t kFsrRasterizerSlotCount =
    D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;

struct FsrRasterizerSnapshot {
    bool rasterizer_known = false;
    bool viewport_known = false;
    bool scissor_known = false;
    bool scissor_enabled = false;
    // Verdadeiro quando este snapshot precisou consultar o estado vivo por
    // faltar shadow utilizavel. Serve de metrica: se nao cair para perto de
    // zero depois dos primeiros frames, algo esta invalidando o shadow.
    bool seeded_from_live_state = false;
    UINT viewport_count = 0;
    UINT scissor_count = 0;
    std::array<D3D11_VIEWPORT, kFsrRasterizerSlotCount> viewports = {};
    std::array<D3D11_RECT, kFsrRasterizerSlotCount> scissors = {};
};

// Alimentados pelos hooks dos respectivos setters. Exigem o catalog lock.
void rasterizer_shadow_record_state_locked(
    ID3D11DeviceContext* context, ID3D11RasterizerState* state);
void rasterizer_shadow_record_viewports_locked(
    ID3D11DeviceContext* context,
    UINT viewport_count,
    const D3D11_VIEWPORT* viewports);
void rasterizer_shadow_record_scissors_locked(
    ID3D11DeviceContext* context,
    UINT scissor_count,
    const D3D11_RECT* scissors);

// Chamavel SEM o catalog lock, que e onde a invalidacao precisa acontecer: um
// setter que nao conseguiu o lock perdeu a atualizacao e o shadow daquele
// contexto virou mentira. Marca apenas o bucket do contexto, nunca a tabela
// inteira, e a proxima captura se recupera sozinha ressemeando.
void rasterizer_shadow_mark_stale(ID3D11DeviceContext* context);

// Devolve o estado de rasterizacao ativo. Quando o contexto ainda nao tem
// shadow utilizavel, semeia uma unica vez a partir do estado vivo -- sob
// demanda, nao a cada draw. Um contexto sem objeto de rasterizer state esta no
// default do D3D11, ou seja ScissorEnable = FALSE. Exige o catalog lock.
void rasterizer_shadow_capture_locked(
    ID3D11DeviceContext* context, FsrRasterizerSnapshot* snapshot);

void rasterizer_shadow_reset_all_locked();
