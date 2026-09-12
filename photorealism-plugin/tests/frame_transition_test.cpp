#include "../src/resource_observer/frame_transition.hpp"

#include <cassert>
#include <cstdio>

using namespace photorealism::observer;
using namespace photorealism::scene_formats;

namespace {

constexpr unsigned kR16G16B16A16Float = 10u;

TargetShape shape(unsigned width, unsigned height, unsigned format) {
    TargetShape result;
    result.width = width;
    result.height = height;
    result.format = format;
    result.samples = 1;
    return result;
}

FrameTransition milestone() {
    FrameTransition transition;
    transition.configure(1920, 1080, 1280, 720);
    return transition;
}

void the_sizes_measured_in_the_game_are_classified_right() {
    const FrameTransition transition = milestone();
    assert(transition.classify(shape(1288, 728, kR8G8B8A8Unorm)) ==
           TargetRole::Internal);
    assert(transition.classify(shape(1366, 684, kR8G8B8A8Unorm)) ==
           TargetRole::Ignored);
    assert(transition.classify(shape(1920, 1080, kB8G8R8A8Unorm)) ==
           TargetRole::Output);
    assert(transition.classify(shape(2048, 2048, kR8G8B8A8Unorm)) ==
           TargetRole::Ignored);
    assert(transition.classify(shape(512, 512, kR8G8B8A8Unorm)) ==
           TargetRole::Ignored);
}

void typeless_is_internal_and_float_or_msaa_are_not() {
    const FrameTransition transition = milestone();
    assert(transition.classify(shape(1288, 728, kR8G8B8A8Typeless)) ==
           TargetRole::Internal);
    assert(transition.classify(shape(1288, 728, kR16G16B16A16Float)) ==
           TargetRole::Ignored);
    TargetShape msaa = shape(1288, 728, kR8G8B8A8Unorm);
    msaa.samples = 4;
    assert(transition.classify(msaa) == TargetRole::Ignored);
}

void an_unconfigured_transition_ignores_everything() {
    FrameTransition transition;
    int texture = 0;
    const TransitionStep step =
        transition.observe(&texture, shape(1920, 1080, kB8G8R8A8Unorm));
    assert(step.role == TargetRole::Ignored);
    assert(step.capture == nullptr);
}

void the_last_internal_before_the_output_is_captured() {
    FrameTransition transition = milestone();
    int geometry = 0;
    int resolved = 0;
    int backbuffer = 0;
    transition.observe(&geometry, shape(1288, 728, kR8G8B8A8Unorm));
    transition.observe(&resolved, shape(1288, 728, kR8G8B8A8Unorm));
    const TransitionStep step =
        transition.observe(&backbuffer, shape(1920, 1080, kB8G8R8A8Unorm));
    assert(step.role == TargetRole::Output);
    assert(step.capture == &resolved);
    assert(transition.pending() == nullptr);
    assert(transition.transitions() == 1);
}

void an_output_bind_before_any_internal_captures_nothing() {
    FrameTransition transition = milestone();
    int backbuffer = 0;
    const TransitionStep step =
        transition.observe(&backbuffer, shape(1920, 1080, kB8G8R8A8Unorm));
    assert(step.capture == nullptr);
    assert(transition.transitions() == 0);
}

void more_output_binds_after_a_capture_do_not_capture_again() {
    FrameTransition transition = milestone();
    int resolved = 0;
    int backbuffer = 0;
    transition.observe(&resolved, shape(1288, 728, kR8G8B8A8Unorm));
    assert(transition.observe(&backbuffer, shape(1920, 1080, kB8G8R8A8Unorm))
               .capture == &resolved);
    assert(transition.observe(&backbuffer, shape(1920, 1080, kB8G8R8A8Unorm))
               .capture == nullptr);
    assert(transition.transitions() == 1);
}

void the_end_of_a_frame_hands_back_what_was_pending() {
    FrameTransition transition = milestone();
    int orphan = 0;
    transition.observe(&orphan, shape(1288, 728, kR8G8B8A8Unorm));
    assert(transition.end_frame() == &orphan);
    assert(transition.pending() == nullptr);
    assert(transition.internal_binds() == 0);
}

void references_always_balance() {
    FrameTransition transition = milestone();
    int textures[3] = {};
    const unsigned widths[] = {1288, 1288, 1920, 1288, 1288, 1288, 1920, 1920};
    const unsigned pick[] = {0, 1, 2, 1, 1, 0, 2, 2};
    int held = 0;
    for (unsigned index = 0; index < 8; ++index) {
        const unsigned height = widths[index] == 1920 ? 1080 : 728;
        const TransitionStep step = transition.observe(
            &textures[pick[index]], shape(widths[index], height, kR8G8B8A8Unorm));
        held += step.acquired != nullptr ? 1 : 0;
        held -= step.released != nullptr ? 1 : 0;
        held -= step.capture != nullptr ? 1 : 0;
    }
    held -= transition.end_frame() != nullptr ? 1 : 0;
    assert(held == 0);
}

void a_pool_that_swaps_textures_every_frame_no_longer_flickers() {
    FrameTransition transition = milestone();
    int pool[2] = {};
    int backbuffer = 0;
    const char* content[2] = {"", ""};

    const char* position_rule[4] = {};
    const char* identity_rule[4] = {};
    for (unsigned frame = 0; frame < 4; ++frame) {
        const unsigned mask_slot = frame % 2;
        const unsigned scene_slot = 1 - mask_slot;
        content[mask_slot] = "bordas";
        transition.observe(&pool[mask_slot], shape(1288, 728, kR8G8B8A8Unorm));
        content[scene_slot] = "cena";
        transition.observe(&pool[scene_slot], shape(1288, 728, kR8G8B8A8Unorm));
        const TransitionStep step =
            transition.observe(&backbuffer, shape(1920, 1080, kB8G8R8A8Unorm));
        position_rule[frame] = content[step.capture == &pool[0] ? 0 : 1];
        identity_rule[frame] = content[0];
        transition.end_frame();
    }

    for (unsigned frame = 0; frame < 4; ++frame) {
        assert(position_rule[frame][0] == 'c');
    }
    assert(identity_rule[0][0] != identity_rule[1][0]);
}

}

int main() {
    the_sizes_measured_in_the_game_are_classified_right();
    typeless_is_internal_and_float_or_msaa_are_not();
    an_unconfigured_transition_ignores_everything();
    the_last_internal_before_the_output_is_captured();
    an_output_bind_before_any_internal_captures_nothing();
    more_output_binds_after_a_capture_do_not_capture_again();
    the_end_of_a_frame_hands_back_what_was_pending();
    references_always_balance();
    a_pool_that_swaps_textures_every_frame_no_longer_flickers();
    std::printf("frame_transition_test ok\n");
    return 0;
}
