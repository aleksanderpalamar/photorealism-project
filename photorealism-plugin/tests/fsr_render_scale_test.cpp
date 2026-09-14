#include "../src/fsr/render_scale.cpp"

#include <cassert>
#include <cstdio>

using namespace photorealism::fsr;

namespace {

void the_milestone_resolution_comes_out_right() {
    const RenderExtent extent = internal_extent(1920, 1080, 0.6667f);
    assert(extent.width == 1280);
    assert(extent.height == 720);
    assert(extent_reduces_work(extent, 1920, 1080));
}

void every_side_lands_on_a_compute_group() {
    const unsigned widths[] = {1920, 2560, 3440, 1366, 1600};
    const float scales[] = {0.5f, 0.58f, 0.6667f, 0.77f, 0.85f};
    for (unsigned width : widths) {
        for (float scale : scales) {
            const RenderExtent extent = internal_extent(width, 1080, scale);
            assert(extent.width % kComputeGroupSize == 0);
            assert(extent.height % kComputeGroupSize == 0);
        }
    }
}

void the_scale_never_leaves_its_range() {
    assert(clamp_scale(0.1f) == kMinimumScale);
    assert(clamp_scale(4.0f) == kMaximumScale);
    assert(clamp_scale(0.75f) == 0.75f);

    const RenderExtent huge = internal_extent(1920, 1080, 9.0f);
    assert(huge.width == 1920 && huge.height == 1080);
    assert(!extent_reduces_work(huge, 1920, 1080));
}

void a_full_scale_pass_is_not_worth_doing() {
    const RenderExtent extent = internal_extent(1920, 1080, 1.0f);
    assert(extent.width == 1920 && extent.height == 1080);
    assert(!extent_reduces_work(extent, 1920, 1080));
}

void the_internal_frame_never_gets_absurdly_small() {
    const RenderExtent extent = internal_extent(640, 480, 0.5f);
    assert(extent.width >= kMinimumInternalSide);
    assert(extent.height >= kMinimumInternalSide);
}

void the_internal_frame_never_exceeds_the_real_one() {
    const unsigned sides[] = {800, 1280, 1920, 2560};
    for (unsigned side : sides) {
        const RenderExtent extent = internal_extent(side, side, 1.0f);
        assert(extent.width <= side);
        assert(extent.height <= side);
    }
}

void a_zero_sized_backbuffer_is_never_upscaled() {
    const RenderExtent extent = internal_extent(0, 0, 0.6667f);
    assert(!extent_reduces_work(extent, 0, 0));
}

void the_dispatch_covers_every_pixel() {
    assert(dispatch_groups(1920) * kComputeGroupSize >= 1920);
    assert(dispatch_groups(1080) * kComputeGroupSize >= 1080);
    assert(dispatch_groups(1) == 1);
    assert(dispatch_groups(8) == 1);
    assert(dispatch_groups(9) == 2);
}

}

int main() {
    the_milestone_resolution_comes_out_right();
    every_side_lands_on_a_compute_group();
    the_scale_never_leaves_its_range();
    a_full_scale_pass_is_not_worth_doing();
    the_internal_frame_never_gets_absurdly_small();
    the_internal_frame_never_exceeds_the_real_one();
    a_zero_sized_backbuffer_is_never_upscaled();
    the_dispatch_covers_every_pixel();
    std::printf("fsr_render_scale_test ok\n");
    return 0;
}
