#include "../src/config/effect_quality.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>

using namespace photorealism;

namespace {

float smoothstep_value(float edge0, float edge1, float x) {
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float linearize_reversed_depth(float raw_depth, float near_plane) {
    return std::max(near_plane, 0.000001f) / std::max(raw_depth, 0.0000001f);
}

float cabin_weight(float raw_depth, float near_plane) {
    const float distance = linearize_reversed_depth(raw_depth, near_plane);
    const float sky = raw_depth <= 0.0000001f ? 0.0f : 1.0f;
    return sky *
           (1.0f - smoothstep_value(
                       kInteriorLightNearStart, kInteriorLightNearEnd, distance));
}

}

int main() {
    const float near_plane = 0.1f;

    assert(cabin_weight(0.91f, near_plane) > 0.99f);

    assert(cabin_weight(near_plane / 1.75f, near_plane) < 0.01f);
    assert(cabin_weight(near_plane / 2.0f, near_plane) < 0.01f);
    assert(cabin_weight(near_plane / 5.9f, near_plane) < 0.01f);

    assert(cabin_weight(0.0f, near_plane) == 0.0f);

    assert(kInteriorLightNearEnd < 1.75f);
    assert(kInteriorLightNearStart > 0.11f);

    std::printf("interior_light_test ok\n");
    return 0;
}
