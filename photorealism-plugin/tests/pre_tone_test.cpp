#include "../src/config/effect_quality.hpp"
#include "../src/passfx/tone_stage.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace photorealism;
using namespace photorealism::passfx;

namespace {

BindShape shape(unsigned count, unsigned format, unsigned width = 1920, unsigned height = 1080) {
    BindShape result;
    result.count = count;
    result.view_format = format;
    result.width = width;
    result.height = height;
    return result;
}

float mirrored_luma(float luma, const PreToneParameters& parameters) {
    const float exposed = luma * parameters.exposure_gain;
    const float exponent = std::max(1.0f + parameters.contrast, 0.1f);
    return 0.18f * std::pow(std::max(exposed, 1e-6f) / 0.18f, exponent);
}

void the_tone_output_follows_the_hdr_target() {
    ToneStage stage;
    assert(!stage.observe(shape(2, 10)));
    assert(!stage.observe(shape(1, 36)));
    assert(!stage.observe(shape(1, 10)));
    assert(stage.previous_is_hdr());
    assert(stage.observe(shape(1, 29)));
    assert(!stage.observe(shape(1, 29)));
    assert(!stage.observe(shape(1, 87)));
}

void a_different_size_or_a_second_target_is_not_the_tone_pass() {
    ToneStage stage;
    stage.observe(shape(1, 10, 960, 540));
    assert(!stage.observe(shape(1, 29)));
    stage.observe(shape(2, 10));
    assert(!stage.observe(shape(1, 29)));
    stage.observe(shape(1, 10));
    stage.observe(shape(1, 28));
    assert(!stage.observe(shape(1, 29)));
}

void the_reference_sets_map_to_gain_and_contrast() {
    Settings settings = {};
    settings.profile_dynamic_contrast = 1.0f;
    assert(!pre_tone_active(pre_tone_parameters(settings)));
    settings.profile_pre_exposure = -1.0f;
    settings.profile_pre_contrast = 0.42f;
    const PreToneParameters first = pre_tone_parameters(settings);
    assert(std::fabs(first.exposure_gain - 0.5f) < 1e-5f);
    assert(std::fabs(first.contrast - 0.42f) < 1e-5f);
    assert(pre_tone_active(first));
    settings.profile_dynamic_contrast = 0.0f;
    settings.profile_pre_exposure = 0.0f;
    assert(!pre_tone_active(pre_tone_parameters(settings)));
}

void contrast_pivots_on_middle_grey() {
    const PreToneParameters parameters = {1.0f, 0.42f};
    assert(std::fabs(mirrored_luma(0.18f, parameters) - 0.18f) < 1e-5f);
    assert(mirrored_luma(0.9f, parameters) > 0.9f);
    assert(mirrored_luma(0.05f, parameters) < 0.05f);
    const PreToneParameters darker = {0.5f, 0.0f};
    assert(std::fabs(mirrored_luma(0.4f, darker) - 0.2f) < 1e-5f);
}

}

int main() {
    the_tone_output_follows_the_hdr_target();
    a_different_size_or_a_second_target_is_not_the_tone_pass();
    the_reference_sets_map_to_gain_and_contrast();
    contrast_pivots_on_middle_grey();
    std::printf("pre_tone_test ok\n");
    return 0;
}
