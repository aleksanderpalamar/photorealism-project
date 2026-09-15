#pragma once

#include "capture_records.hpp"

#include <d3d11.h>

#include <vector>

namespace photorealism {
namespace frame_capture {

std::vector<TargetInfo> describe_binding(
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    ID3D11DepthStencilView* depth_target);

}
}
