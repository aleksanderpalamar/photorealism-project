#include "../src/fsr/easu_constants.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>

using namespace photorealism::fsr;

namespace {

void the_constants_match_what_amd_computes_for_the_milestone() {
    const EasuConstants constants =
        populate_easu_constants(1288.0f, 728.0f, 1920.0f, 1080.0f);
    assert(constants.con0[0] == 0.670833349f);
    assert(constants.con0[1] == 0.674074054f);
    assert(constants.con0[2] == -0.164583325f);
    assert(constants.con0[3] == -0.162962973f);
    assert(constants.input_size[0] == 1288.0f);
    assert(constants.input_size[1] == 728.0f);
    assert(constants.output_size[0] == 1920.0f);
    assert(constants.output_size[1] == 1080.0f);
}

void every_output_pixel_center_lands_on_its_input_position() {
    const EasuConstants constants =
        populate_easu_constants(1288.0f, 728.0f, 1920.0f, 1080.0f);
    const float corners[] = {0.0f, 1.0f, 959.0f, 1919.0f};
    for (float ip : corners) {
        const float pp = ip * constants.con0[0] + constants.con0[2];
        const float expected = (ip + 0.5f) * (1288.0f / 1920.0f) - 0.5f;
        assert(std::fabs(pp - expected) < 1e-3f);
        assert(pp > -0.5f && pp < 1288.0f - 0.5f);
    }
}

void the_layout_is_the_one_the_shader_reads() {
    assert(sizeof(EasuConstants) == 32);
    assert(offsetof(EasuConstants, con0) == 0);
    assert(offsetof(EasuConstants, input_size) == 16);
    assert(offsetof(EasuConstants, output_size) == 24);
}

}

int main() {
    the_constants_match_what_amd_computes_for_the_milestone();
    every_output_pixel_center_lands_on_its_input_position();
    the_layout_is_the_one_the_shader_reads();
    std::printf("fsr_easu_constants_test ok\n");
    return 0;
}
