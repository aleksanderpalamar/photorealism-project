#include "../src/depth_scoring.hpp"

#include <cassert>

int main() {
    using photorealism::depth_scoring::is_confident_candidate;
    using photorealism::depth_scoring::is_scene_candidate;
    using photorealism::depth_scoring::resource_score;

    constexpr unsigned backbuffer_width = 1920;
    constexpr unsigned backbuffer_height = 1080;

    const auto ets2_interface = resource_score(
        1920, 1080, 7499, backbuffer_width, backbuffer_height);
    const auto ets2_world = resource_score(
        1920, 2160, 32818, backbuffer_width, backbuffer_height);
    const auto ets2_shadow = resource_score(
        4096, 4096, 7236, backbuffer_width, backbuffer_height);
    assert(ets2_world > ets2_interface);
    assert(ets2_world > ets2_shadow);
    assert(!is_scene_candidate(
        1920, 1080, 7499, 1, backbuffer_width, backbuffer_height, 30000));
    assert(is_scene_candidate(
        1920, 2160, 32818, 1, backbuffer_width, backbuffer_height, 30000));

    const auto ats_interface = resource_score(
        1920, 1080, 140, backbuffer_width, backbuffer_height);
    const auto ats_world = resource_score(
        2400, 1350, 30000, backbuffer_width, backbuffer_height);
    assert(ats_world > ats_interface);
    assert(!is_confident_candidate(
        1920, 1080, 140, 1, backbuffer_width, backbuffer_height));
    assert(is_confident_candidate(
        2400, 1350, 30000, 1, backbuffer_width, backbuffer_height));
    assert(is_scene_candidate(
        2400, 1350, 30000, 1, backbuffer_width, backbuffer_height, 30000));

    assert(is_scene_candidate(
        1920, 1080, 15000, 1, backbuffer_width, backbuffer_height, 30000));
    assert(!is_scene_candidate(
        1920, 1080, 7499, 1, backbuffer_width, backbuffer_height, 30000));
    assert(is_scene_candidate(
        1920, 2160, 1000, 1, backbuffer_width, backbuffer_height, 3000));

    assert(!is_confident_candidate(
        1920, 2160, 32818, 4, backbuffer_width, backbuffer_height));
    assert(!is_scene_candidate(
        1920, 2160, 32818, 4, backbuffer_width, backbuffer_height, 30000));
    return 0;
}
