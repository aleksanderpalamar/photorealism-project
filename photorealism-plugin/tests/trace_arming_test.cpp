#include "../src/resource_observer/trace_arming.hpp"

#include <cassert>
#include <cstdio>

using namespace photorealism::observer;

namespace {

constexpr unsigned kLoadingScreenBinds = 3;
constexpr unsigned kDrivingBinds = 72;

void the_loading_screen_never_arms_the_trace() {
    TraceArming arming;
    for (unsigned frame = 0; frame < 5000; ++frame) {
        assert(!arming.end_frame(kLoadingScreenBinds));
    }
}

void the_map_arms_only_after_it_settles() {
    TraceArming arming;
    for (unsigned frame = 1; frame < kSettledSceneFrames; ++frame) {
        assert(!arming.end_frame(kDrivingBinds));
    }
    assert(arming.end_frame(kDrivingBinds));
}

void a_loading_frame_in_the_middle_starts_the_count_over() {
    TraceArming arming;
    for (unsigned frame = 0; frame < kSettledSceneFrames - 1; ++frame) {
        arming.end_frame(kDrivingBinds);
    }
    assert(!arming.end_frame(kLoadingScreenBinds));
    assert(arming.settled() == 0);
    assert(!arming.end_frame(kDrivingBinds));
}

void the_threshold_sits_between_the_measured_frames() {
    assert(kLoadingScreenBinds < kSceneBinds);
    assert(kDrivingBinds >= kSceneBinds);
}

}

int main() {
    the_loading_screen_never_arms_the_trace();
    the_map_arms_only_after_it_settles();
    a_loading_frame_in_the_middle_starts_the_count_over();
    the_threshold_sits_between_the_measured_frames();
    std::printf("trace_arming_test ok\n");
    return 0;
}
