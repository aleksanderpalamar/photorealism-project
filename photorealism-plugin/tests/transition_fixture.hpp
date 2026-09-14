#pragma once

#include "../src/resource_observer/frame_transition.hpp"

namespace transition_fixture {

using photorealism::observer::FrameTransition;
using photorealism::observer::TargetShape;

constexpr unsigned kR16G16B16A16Float = 10u;

inline int g_backbuffer = 0;

inline TargetShape shape(unsigned width, unsigned height, unsigned format) {
    TargetShape result;
    result.width = width;
    result.height = height;
    result.format = format;
    result.samples = 1;
    return result;
}

inline TargetShape internal_shape() {
    return shape(1288, 728, photorealism::scene_formats::kR8G8B8A8Unorm);
}

inline TargetShape output_shape() {
    return shape(1920, 1080, photorealism::scene_formats::kB8G8R8A8Unorm);
}

inline FrameTransition milestone() {
    FrameTransition transition;
    transition.configure(&g_backbuffer, 1280, 720);
    return transition;
}

}
