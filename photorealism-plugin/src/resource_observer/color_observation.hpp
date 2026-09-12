#pragma once

#include <d3d11.h>

namespace photorealism {

void enable_color_capture(
    UINT output_width,
    UINT output_height,
    UINT expected_width,
    UINT expected_height);
void disable_color_capture();
void observe_color_targets(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    ID3D11DepthStencilView* depth_target);
bool acquire_captured_frame(
    ID3D11ShaderResourceView** view, UINT* width, UINT* height);
void end_color_frame();
void reset_color_discovery();

}
