#include "color_observation.hpp"

#include "../postprocess/com_utils.hpp"
#include "../postprocess/format_utils.hpp"
#include "../runtime.hpp"

#include <windows.h>

namespace photorealism {
namespace {

constexpr UINT kMaximumColorCandidates = 16;
constexpr UINT kMinimumSideFraction = 2;

struct ColorCandidate {
    ID3D11Texture2D* texture;
    D3D11_TEXTURE2D_DESC description;
    unsigned long long bindings;
};

SRWLOCK g_color_lock = SRWLOCK_INIT;
ColorCandidate g_candidates[kMaximumColorCandidates] = {};
UINT g_candidate_count = 0;
UINT g_output_width = 0;
UINT g_output_height = 0;
ID3D11Texture2D* g_reported = nullptr;

bool inside_search_window(const D3D11_TEXTURE2D_DESC& description) {
    if (g_output_width == 0 || g_output_height == 0) {
        return false;
    }
    if (description.SampleDesc.Count != 1 ||
        !is_supported_format(description.Format)) {
        return false;
    }
    if (description.Width >= g_output_width ||
        description.Height >= g_output_height) {
        return false;
    }
    return description.Width * kMinimumSideFraction >= g_output_width &&
           description.Height * kMinimumSideFraction >= g_output_height;
}

void remember(ID3D11Texture2D* texture, const D3D11_TEXTURE2D_DESC& described) {
    for (UINT index = 0; index < g_candidate_count; ++index) {
        if (g_candidates[index].texture != texture) {
            continue;
        }
        ++g_candidates[index].bindings;
        return;
    }
    if (g_candidate_count >= kMaximumColorCandidates) {
        return;
    }
    texture->AddRef();
    g_candidates[g_candidate_count].texture = texture;
    g_candidates[g_candidate_count].description = described;
    g_candidates[g_candidate_count].bindings = 1;
    ++g_candidate_count;
}

void inspect(ID3D11RenderTargetView* view) {
    if (view == nullptr) {
        return;
    }
    ID3D11Resource* resource = nullptr;
    view->GetResource(&resource);
    if (resource == nullptr) {
        return;
    }
    ID3D11Texture2D* texture = nullptr;
    resource->QueryInterface(
        IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture));
    resource->Release();
    if (texture == nullptr) {
        return;
    }

    D3D11_TEXTURE2D_DESC described = {};
    texture->GetDesc(&described);
    if (inside_search_window(described)) {
        remember(texture, described);
    }
    texture->Release();
}

}

void set_color_search_window(UINT width, UINT height) {
    AcquireSRWLockExclusive(&g_color_lock);
    g_output_width = width;
    g_output_height = height;
    ReleaseSRWLockExclusive(&g_color_lock);
}

void observe_color_targets(
    UINT render_target_count, ID3D11RenderTargetView* const* render_targets) {
    if (render_targets == nullptr || render_target_count == 0) {
        return;
    }
    AcquireSRWLockExclusive(&g_color_lock);
    for (UINT index = 0; index < render_target_count; ++index) {
        inspect(render_targets[index]);
    }
    ReleaseSRWLockExclusive(&g_color_lock);
}

bool acquire_color_candidate(
    ID3D11Texture2D** texture, D3D11_TEXTURE2D_DESC* description) {
    if (texture == nullptr || description == nullptr) {
        return false;
    }
    AcquireSRWLockExclusive(&g_color_lock);
    UINT best = kMaximumColorCandidates;
    unsigned long long most = 0;
    for (UINT index = 0; index < g_candidate_count; ++index) {
        if (g_candidates[index].bindings <= most) {
            continue;
        }
        most = g_candidates[index].bindings;
        best = index;
    }
    if (best == kMaximumColorCandidates) {
        ReleaseSRWLockExclusive(&g_color_lock);
        return false;
    }

    *texture = g_candidates[best].texture;
    *description = g_candidates[best].description;
    (*texture)->AddRef();
    const bool first_time = g_reported != *texture;
    g_reported = *texture;
    ReleaseSRWLockExclusive(&g_color_lock);

    if (first_time) {
        log_message(
            "FSR achou o quadro interno do jogo: %ux%u formato=%u, ligado %llu "
            "vezes, entre %u candidatos na janela de busca.",
            description->Width,
            description->Height,
            static_cast<unsigned>(description->Format),
            most,
            g_candidate_count);
    }
    return true;
}

void reset_color_discovery() {
    AcquireSRWLockExclusive(&g_color_lock);
    for (UINT index = 0; index < g_candidate_count; ++index) {
        safe_release(g_candidates[index].texture);
    }
    g_candidate_count = 0;
    g_reported = nullptr;
    ReleaseSRWLockExclusive(&g_color_lock);
}

}
