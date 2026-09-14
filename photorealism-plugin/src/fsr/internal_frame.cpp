#include "internal_frame.hpp"

#include "../postprocess/com_utils.hpp"
#include "../resource_observer/color_observation.hpp"

namespace photorealism {
namespace fsr {

bool InternalFrame::acquire() {
    release();
    UINT width = 0;
    UINT height = 0;
    if (!acquire_captured_frame(&view_, &width, &height)) {
        return false;
    }
    extent_.width = width;
    extent_.height = height;
    return true;
}

void InternalFrame::release() {
    safe_release(view_);
}

}
}
