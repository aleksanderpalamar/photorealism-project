#include "render_scale.hpp"

namespace photorealism {
namespace fsr {
namespace {

unsigned round_down_to_group(unsigned value) {
    return value - value % kComputeGroupSize;
}

unsigned scaled_side(unsigned side, float scale) {
    if (side == 0) {
        return 0;
    }
    const float scaled = static_cast<float>(side) * scale;
    const unsigned rounded = round_down_to_group(static_cast<unsigned>(scaled));
    if (rounded < kMinimumInternalSide) {
        return side < kMinimumInternalSide ? side : kMinimumInternalSide;
    }
    return rounded > side ? side : rounded;
}

}

float clamp_scale(float scale) {
    if (scale < kMinimumScale) {
        return kMinimumScale;
    }
    return scale > kMaximumScale ? kMaximumScale : scale;
}

RenderExtent internal_extent(unsigned width, unsigned height, float scale) {
    const float safe = clamp_scale(scale);
    RenderExtent extent = {};
    extent.width = scaled_side(width, safe);
    extent.height = scaled_side(height, safe);
    return extent;
}

bool extent_reduces_work(
    const RenderExtent& internal, unsigned width, unsigned height) {
    if (internal.width == 0 || internal.height == 0) {
        return false;
    }
    if (internal.width > width || internal.height > height) {
        return false;
    }
    return internal.width < width || internal.height < height;
}

unsigned dispatch_groups(unsigned extent) {
    return (extent + kComputeGroupSize - 1) / kComputeGroupSize;
}

}
}
