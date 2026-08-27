#include "fsr_rasterizer_shadow.hpp"

#include "fsr_pointer_hash.hpp"

#include <atomic>
#include <cstring>

namespace {

// Potencia de dois: pointer_bucket depende disso para mascarar.
constexpr std::size_t kContextCapacity = 16;

struct RasterizerShadowEntry {
    ID3D11DeviceContext* context = nullptr;
    std::array<D3D11_VIEWPORT, kFsrRasterizerSlotCount> viewports = {};
    std::array<D3D11_RECT, kFsrRasterizerSlotCount> scissors = {};
    UINT viewport_count = 0;
    UINT scissor_count = 0;
    std::uint32_t generation = 0;
    bool occupied = false;
    bool rasterizer_known = false;
    bool viewport_known = false;
    bool scissor_known = false;
    bool scissor_enabled = false;
};

std::array<RasterizerShadowEntry, kContextCapacity> g_entries = {};

// Uma geracao por bucket, e nao uma global: perder uma atualizacao de um
// contexto nao pode invalidar o shadow de todos os outros. Colisao de hash so
// causa uma ressemeadura extra, que e conservadora e inofensiva.
std::array<std::atomic<std::uint32_t>, kContextCapacity> g_generations = {};

std::size_t bucket_of(ID3D11DeviceContext* context) {
    return photorealism::fsr::pointer_bucket(
        reinterpret_cast<std::uintptr_t>(context), kContextCapacity);
}

std::uint32_t current_generation(ID3D11DeviceContext* context) {
    return g_generations[bucket_of(context)].load(std::memory_order_acquire);
}

RasterizerShadowEntry* find_entry_locked(ID3D11DeviceContext* context) {
    const std::size_t base = bucket_of(context);
    for (std::size_t probe = 0; probe < kContextCapacity; ++probe) {
        RasterizerShadowEntry& entry =
            g_entries[(base + probe) & (kContextCapacity - 1)];
        if (entry.occupied && entry.context == context) {
            return &entry;
        }
    }
    return nullptr;
}

RasterizerShadowEntry* acquire_entry_locked(ID3D11DeviceContext* context) {
    const std::size_t base = bucket_of(context);
    RasterizerShadowEntry* free_slot = nullptr;
    for (std::size_t probe = 0; probe < kContextCapacity; ++probe) {
        RasterizerShadowEntry& entry =
            g_entries[(base + probe) & (kContextCapacity - 1)];
        if (entry.occupied && entry.context == context) {
            return &entry;
        }
        if (!entry.occupied && free_slot == nullptr) {
            free_slot = &entry;
        }
    }
    if (free_slot == nullptr) {
        return nullptr;
    }
    *free_slot = {};
    free_slot->context = context;
    free_slot->occupied = true;
    free_slot->generation = current_generation(context);
    return free_slot;
}

// Descarta o conteudo de uma entrada cuja geracao ficou para tras, sem
// devolver o slot: o contexto continua sendo o mesmo, so o estado envelheceu.
RasterizerShadowEntry* prepare_entry_locked(ID3D11DeviceContext* context) {
    RasterizerShadowEntry* entry = acquire_entry_locked(context);
    if (entry == nullptr) {
        return nullptr;
    }
    const std::uint32_t generation = current_generation(context);
    if (entry->generation != generation) {
        *entry = {};
        entry->context = context;
        entry->occupied = true;
        entry->generation = generation;
    }
    return entry;
}

bool slot_count_is_valid(UINT count, const void* values) {
    return count <= kFsrRasterizerSlotCount &&
           (count == 0 || values != nullptr);
}

// Le o estado vivo do contexto para dentro do snapshot. Um contexto sem objeto
// de rasterizer state esta no default do D3D11, cujo ScissorEnable e FALSE --
// a mesma leitura que a prova fazia antes do shadow existir.
void read_live_state(
    ID3D11DeviceContext* context, FsrRasterizerSnapshot* snapshot) {
    ID3D11RasterizerState* state = nullptr;
    context->RSGetState(&state);
    D3D11_RASTERIZER_DESC description = {};
    if (state != nullptr) {
        state->GetDesc(&description);
        state->Release();
    }
    snapshot->rasterizer_known = true;
    snapshot->scissor_enabled = description.ScissorEnable != FALSE;

    UINT viewport_count = kFsrRasterizerSlotCount;
    context->RSGetViewports(&viewport_count, snapshot->viewports.data());
    snapshot->viewport_count =
        viewport_count <= kFsrRasterizerSlotCount ? viewport_count : 0;
    snapshot->viewport_known = true;

    UINT scissor_count = kFsrRasterizerSlotCount;
    context->RSGetScissorRects(&scissor_count, snapshot->scissors.data());
    snapshot->scissor_count =
        scissor_count <= kFsrRasterizerSlotCount ? scissor_count : 0;
    snapshot->scissor_known = true;
}

void store_seed_locked(
    ID3D11DeviceContext* context, const FsrRasterizerSnapshot& snapshot) {
    RasterizerShadowEntry* entry = acquire_entry_locked(context);
    if (entry == nullptr) {
        return;
    }
    entry->generation = current_generation(context);
    entry->rasterizer_known = snapshot.rasterizer_known;
    entry->viewport_known = snapshot.viewport_known;
    entry->scissor_known = snapshot.scissor_known;
    entry->scissor_enabled = snapshot.scissor_enabled;
    entry->viewport_count = snapshot.viewport_count;
    entry->scissor_count = snapshot.scissor_count;
    entry->viewports = snapshot.viewports;
    entry->scissors = snapshot.scissors;
}

}  // namespace

void rasterizer_shadow_record_state_locked(
    ID3D11DeviceContext* context, ID3D11RasterizerState* state) {
    if (context == nullptr) {
        return;
    }
    RasterizerShadowEntry* entry = prepare_entry_locked(context);
    if (entry == nullptr) {
        return;
    }
    D3D11_RASTERIZER_DESC description = {};
    if (state != nullptr) {
        state->GetDesc(&description);
    }
    entry->rasterizer_known = true;
    entry->scissor_enabled = description.ScissorEnable != FALSE;
}

void rasterizer_shadow_record_viewports_locked(
    ID3D11DeviceContext* context,
    UINT viewport_count,
    const D3D11_VIEWPORT* viewports) {
    if (context == nullptr || !slot_count_is_valid(viewport_count, viewports)) {
        return;
    }
    RasterizerShadowEntry* entry = prepare_entry_locked(context);
    if (entry == nullptr) {
        return;
    }
    entry->viewports = {};
    entry->viewport_count = viewport_count;
    entry->viewport_known = true;
    if (viewport_count != 0) {
        std::memcpy(
            entry->viewports.data(),
            viewports,
            sizeof(D3D11_VIEWPORT) * viewport_count);
    }
}

void rasterizer_shadow_record_scissors_locked(
    ID3D11DeviceContext* context,
    UINT scissor_count,
    const D3D11_RECT* scissors) {
    if (context == nullptr || !slot_count_is_valid(scissor_count, scissors)) {
        return;
    }
    RasterizerShadowEntry* entry = prepare_entry_locked(context);
    if (entry == nullptr) {
        return;
    }
    entry->scissors = {};
    entry->scissor_count = scissor_count;
    entry->scissor_known = true;
    if (scissor_count != 0) {
        std::memcpy(
            entry->scissors.data(),
            scissors,
            sizeof(D3D11_RECT) * scissor_count);
    }
}

void rasterizer_shadow_mark_stale(ID3D11DeviceContext* context) {
    if (context == nullptr) {
        return;
    }
    g_generations[bucket_of(context)].fetch_add(1u, std::memory_order_acq_rel);
}

void rasterizer_shadow_capture_locked(
    ID3D11DeviceContext* context, FsrRasterizerSnapshot* snapshot) {
    if (snapshot == nullptr) {
        return;
    }
    *snapshot = {};
    if (context == nullptr) {
        return;
    }

    const RasterizerShadowEntry* entry = find_entry_locked(context);
    if (entry != nullptr && entry->generation == current_generation(context) &&
        entry->rasterizer_known && entry->viewport_known &&
        entry->scissor_known) {
        snapshot->rasterizer_known = true;
        snapshot->viewport_known = true;
        snapshot->scissor_known = true;
        snapshot->scissor_enabled = entry->scissor_enabled;
        snapshot->viewport_count = entry->viewport_count;
        snapshot->scissor_count = entry->scissor_count;
        snapshot->viewports = entry->viewports;
        snapshot->scissors = entry->scissors;
        return;
    }

    // Semeadura sob demanda. A geracao e lida antes e depois: se um setter
    // perdeu o lock no meio da leitura, o seed pode ja estar obsoleto e nao
    // deve ser guardado -- so entregue a este draw, que o observou de fato.
    const std::uint32_t before = current_generation(context);
    read_live_state(context, snapshot);
    snapshot->seeded_from_live_state = true;
    if (current_generation(context) == before) {
        store_seed_locked(context, *snapshot);
    }
}

void rasterizer_shadow_reset_all_locked() {
    g_entries = {};
    for (std::atomic<std::uint32_t>& generation : g_generations) {
        generation.fetch_add(1u, std::memory_order_acq_rel);
    }
}
