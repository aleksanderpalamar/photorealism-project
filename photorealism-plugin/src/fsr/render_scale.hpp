#pragma once

namespace photorealism {
namespace fsr {

constexpr unsigned kComputeGroupSize = 8;
constexpr unsigned kMinimumInternalSide = 256;
constexpr float kMinimumScale = 0.50f;
constexpr float kMaximumScale = 1.00f;

struct RenderExtent {
    unsigned width = 0;
    unsigned height = 0;
};

float clamp_scale(float scale);

RenderExtent internal_extent(unsigned width, unsigned height, float scale);

bool extent_reduces_work(
    const RenderExtent& internal, unsigned width, unsigned height);

unsigned dispatch_groups(unsigned extent);

}
}
