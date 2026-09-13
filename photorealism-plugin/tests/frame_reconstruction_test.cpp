#include "transition_fixture.hpp"

#include <cassert>
#include <cstdio>

using namespace photorealism::observer;
using namespace photorealism::scene_formats;
using namespace transition_fixture;

namespace {

void the_bind_that_captures_does_not_reconstruct() {
    FrameTransition transition = milestone();
    int resolved = 0;
    transition.observe(&resolved, internal_shape());
    const TransitionStep upscale = transition.observe(&g_backbuffer, output_shape());
    assert(upscale.capture == &resolved);
    assert(!upscale.reconstruct);
}

void the_second_backbuffer_bind_reconstructs_once() {
    FrameTransition transition = milestone();
    int resolved = 0;
    transition.observe(&resolved, internal_shape());
    transition.observe(&g_backbuffer, output_shape());
    const TransitionStep interface = transition.observe(&g_backbuffer, output_shape());
    assert(interface.capture == nullptr);
    assert(interface.reconstruct);
    const TransitionStep later = transition.observe(&g_backbuffer, output_shape());
    assert(later.capture == nullptr);
    assert(!later.reconstruct);
    assert(transition.transitions() == 1);
}

void a_frame_with_a_single_backbuffer_bind_never_reconstructs() {
    FrameTransition transition = milestone();
    int resolved = 0;
    transition.observe(&resolved, internal_shape());
    transition.observe(&g_backbuffer, output_shape());
    assert(!transition.reconstructed());
    transition.end_frame();
    assert(!transition.observe(&g_backbuffer, output_shape()).reconstruct);
}

void a_full_size_target_after_the_capture_never_reconstructs() {
    FrameTransition transition = milestone();
    int resolved = 0;
    int full_size_target = 0;
    transition.observe(&resolved, internal_shape());
    transition.observe(&g_backbuffer, output_shape());
    assert(!transition.observe(&full_size_target, output_shape()).reconstruct);
    assert(transition.observe(&g_backbuffer, output_shape()).reconstruct);
}

void every_frame_reconstructs_again_after_the_present() {
    FrameTransition transition = milestone();
    int resolved = 0;
    for (unsigned frame = 0; frame < 3; ++frame) {
        transition.observe(&resolved, internal_shape());
        transition.observe(&g_backbuffer, output_shape());
        assert(transition.observe(&g_backbuffer, output_shape()).reconstruct);
        transition.end_frame();
        assert(!transition.reconstructed());
        assert(transition.binds() == 0);
    }
}

void the_frame_measured_in_the_game_reconstructs_before_the_interface() {
    FrameTransition transition = milestone();
    int edges = 0;
    int weights = 0;
    int lighting = 0;
    int tonemapped = 0;
    const TransitionStep early = transition.observe(&g_backbuffer, output_shape());
    assert(!early.reconstruct && early.capture == nullptr);
    transition.observe(&edges, internal_shape());
    transition.observe(&weights, internal_shape());
    transition.observe(&lighting, shape(1288, 728, kR16G16B16A16Float));
    transition.observe(&tonemapped, shape(1288, 728, kR8G8B8A8UnormSrgb));
    transition.observe(&tonemapped, shape(1288, 728, kR8G8B8A8UnormSrgb));
    const TransitionStep upscale = transition.observe(&g_backbuffer, output_shape());
    const TransitionStep interface = transition.observe(&g_backbuffer, output_shape());
    const TransitionStep overlay = transition.observe(&g_backbuffer, output_shape());
    assert(upscale.capture == &tonemapped);
    assert(interface.reconstruct);
    assert(!overlay.reconstruct);
    assert(transition.binds() == 9);
}

}

int main() {
    the_bind_that_captures_does_not_reconstruct();
    the_second_backbuffer_bind_reconstructs_once();
    a_frame_with_a_single_backbuffer_bind_never_reconstructs();
    a_full_size_target_after_the_capture_never_reconstructs();
    every_frame_reconstructs_again_after_the_present();
    the_frame_measured_in_the_game_reconstructs_before_the_interface();
    std::printf("frame_reconstruction_test ok\n");
    return 0;
}
