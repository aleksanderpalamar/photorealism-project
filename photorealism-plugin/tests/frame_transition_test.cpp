#include "transition_fixture.hpp"

#include <cassert>
#include <cstdio>

using namespace photorealism::observer;
using namespace photorealism::scene_formats;
using namespace transition_fixture;

namespace {

void the_sizes_measured_in_the_game_are_classified_right() {
    const FrameTransition transition = milestone();
    int other = 0;
    assert(transition.classify(&other, internal_shape()) ==
           TargetRole::Internal);
    assert(transition.classify(&other, shape(1366, 684, kR8G8B8A8Unorm)) ==
           TargetRole::Ignored);
    assert(transition.classify(&g_backbuffer, output_shape()) ==
           TargetRole::Output);
    assert(transition.classify(&other, shape(2048, 2048, kR8G8B8A8Unorm)) ==
           TargetRole::Ignored);
    assert(transition.classify(&other, shape(512, 512, kR8G8B8A8Unorm)) ==
           TargetRole::Ignored);
}

void only_the_backbuffer_is_output_not_any_target_of_its_size() {
    const FrameTransition transition = milestone();
    int full_size_target = 0;
    assert(transition.classify(&full_size_target, output_shape()) ==
           TargetRole::Ignored);
}

void typeless_is_internal_and_float_or_msaa_are_not() {
    const FrameTransition transition = milestone();
    int other = 0;
    assert(transition.classify(&other, shape(1288, 728, kR8G8B8A8Typeless)) ==
           TargetRole::Internal);
    assert(transition.classify(&other, shape(1288, 728, kR16G16B16A16Float)) ==
           TargetRole::Ignored);
    TargetShape msaa = internal_shape();
    msaa.samples = 4;
    assert(transition.classify(&other, msaa) == TargetRole::Ignored);
}

void an_unconfigured_transition_ignores_everything() {
    FrameTransition transition;
    const TransitionStep step = transition.observe(&g_backbuffer, output_shape());
    assert(step.role == TargetRole::Ignored);
    assert(step.capture == nullptr);
    assert(!step.reconstruct);
}

void the_last_internal_before_the_output_is_captured() {
    FrameTransition transition = milestone();
    int geometry = 0;
    int resolved = 0;
    transition.observe(&geometry, internal_shape());
    transition.observe(&resolved, internal_shape());
    const TransitionStep step = transition.observe(&g_backbuffer, output_shape());
    assert(step.role == TargetRole::Output);
    assert(step.capture == &resolved);
    assert(transition.pending() == nullptr);
    assert(transition.transitions() == 1);
}

void an_output_bind_before_any_internal_does_nothing() {
    FrameTransition transition = milestone();
    const TransitionStep step = transition.observe(&g_backbuffer, output_shape());
    assert(step.capture == nullptr);
    assert(!step.reconstruct);
    assert(transition.transitions() == 0);
}

void the_end_of_a_frame_hands_back_what_was_pending() {
    FrameTransition transition = milestone();
    int orphan = 0;
    transition.observe(&orphan, internal_shape());
    assert(transition.end_frame() == &orphan);
    assert(transition.pending() == nullptr);
    assert(transition.internal_binds() == 0);
}

void references_always_balance() {
    FrameTransition transition = milestone();
    int textures[2] = {};
    const bool output[] = {false, false, true, false, false, false, true, true};
    const unsigned pick[] = {0, 1, 0, 1, 1, 0, 0, 0};
    int held = 0;
    for (unsigned index = 0; index < 8; ++index) {
        const TransitionStep step = output[index]
            ? transition.observe(&g_backbuffer, output_shape())
            : transition.observe(&textures[pick[index]], internal_shape());
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
    const char* content[2] = {"", ""};

    const char* position_rule[4] = {};
    const char* identity_rule[4] = {};
    for (unsigned frame = 0; frame < 4; ++frame) {
        const unsigned mask_slot = frame % 2;
        const unsigned scene_slot = 1 - mask_slot;
        content[mask_slot] = "bordas";
        transition.observe(&pool[mask_slot], internal_shape());
        content[scene_slot] = "cena";
        transition.observe(&pool[scene_slot], internal_shape());
        const TransitionStep step =
            transition.observe(&g_backbuffer, output_shape());
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
    only_the_backbuffer_is_output_not_any_target_of_its_size();
    typeless_is_internal_and_float_or_msaa_are_not();
    an_unconfigured_transition_ignores_everything();
    the_last_internal_before_the_output_is_captured();
    an_output_bind_before_any_internal_does_nothing();
    the_end_of_a_frame_hands_back_what_was_pending();
    references_always_balance();
    a_pool_that_swaps_textures_every_frame_no_longer_flickers();
    std::printf("frame_transition_test ok\n");
    return 0;
}
