#pragma once

#include <d3d11.h>

namespace photorealism {

void set_color_search_window(UINT width, UINT height);
void observe_color_targets(
    UINT render_target_count, ID3D11RenderTargetView* const* render_targets);
bool acquire_color_candidate(
    ID3D11Texture2D** texture, D3D11_TEXTURE2D_DESC* description);
void reset_color_discovery();

}
