#include "../src/fsr/rcas_constants.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>

using namespace photorealism::fsr;

namespace {

float con_for(float sharpness) {
    return rcas_con_from_stops(rcas_stops_from_sharpness(sharpness));
}

void the_slider_follows_the_fsr2_api_remap() {
    assert(rcas_stops_from_sharpness(1.0f) == 0.0f);
    assert(rcas_stops_from_sharpness(0.5f) == 1.0f);
    assert(rcas_stops_from_sharpness(0.0f) == 2.0f);
    assert(con_for(1.0f) == 1.0f);
    assert(con_for(0.5f) == 0.5f);
    assert(con_for(0.0f) == 0.25f);
    assert(std::fabs(con_for(0.60f) - 0.574349177f) < 1e-6f);
}

void the_grain_phase_stays_in_the_unit_interval() {
    for (unsigned frame = 0; frame < 100000; frame += 7) {
        const float phase = grain_phase(frame);
        assert(phase >= 0.0f && phase < 1.0f);
    }
    assert(grain_phase(0) == 0.0f);
}

void the_layout_is_the_one_the_shader_reads() {
    assert(sizeof(RcasConstants) == 32);
    assert(offsetof(RcasConstants, rcas_con) == 0);
    assert(offsetof(RcasConstants, decode_before_write) == 4);
    assert(offsetof(RcasConstants, grain_amount) == 8);
    assert(offsetof(RcasConstants, grain_phase) == 12);
    assert(offsetof(RcasConstants, output_size) == 16);
    assert(offsetof(RcasConstants, grain_tile_size) == 24);
}

}

int main() {
    the_slider_follows_the_fsr2_api_remap();
    the_grain_phase_stays_in_the_unit_interval();
    the_layout_is_the_one_the_shader_reads();
    std::printf("fsr_rcas_constants_test ok\n");
    return 0;
}
