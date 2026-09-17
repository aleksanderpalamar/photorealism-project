#pragma once

#include <d3d11.h>

#include "../config/settings.hpp"

namespace photorealism {
namespace shader_patch {

constexpr UINT kSurfaceConstantSlot = 13;

struct SurfaceConstants {
    float rain[4];
    float road[4];
    float frame[4];
    float mask[4];
    float surface[4];
};

static_assert(
    sizeof(SurfaceConstants) == 80,
    "o buffer de superficie precisa de 80 bytes alinhados");

bool create_surface_constants(ID3D11Device* device);
void release_surface_constants();

void update_surface_constants(const Settings& settings, UINT width, UINT height);
void neutralize_surface_constants();

void bind_surface_constants(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets);

bool surface_constants_ready();
unsigned surface_bind_count();

}
}
