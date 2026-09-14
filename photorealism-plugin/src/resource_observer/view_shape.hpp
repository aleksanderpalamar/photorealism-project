#pragma once

#include "frame_transition.hpp"

#include <d3d11.h>

namespace photorealism {
namespace observer {

bool describe_view(ID3D11View* view, ID3D11Texture2D** texture, TargetShape* shape);
void release_texture_handle(void* handle);

}
}
