#pragma once

#include <d3d11.h>

namespace photorealism {

void request_frame_capture();
const char* frame_capture_status();
void observe_capture_binds(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    ID3D11DepthStencilView* depth_target);
void end_capture_frame();

}
