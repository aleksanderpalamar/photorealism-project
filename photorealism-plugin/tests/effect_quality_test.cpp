#include "../src/config/effect_quality.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace photorealism;

namespace {

Settings neutral() {
    Settings settings = {};
    settings.ssao_intensity_scale = 1.5f;
    settings.profile_taa = 2.0f;
    settings.profile_fxaa = 1.0f;
    settings.profile_use_interior_lighting = 1.0f;
    settings.profile_lighting_interior = 0.17f;
    return settings;
}

void levels_round_and_clamp() {
    assert(quality_level(0.0f) == 0);
    assert(quality_level(0.6f) == 1);
    assert(quality_level(2.0f) == 2);
    assert(quality_level(9.0f) == 2);
    assert(quality_level(-3.0f) == 0);
}

void anti_aliasing_follows_the_list() {
    Settings settings = neutral();
    settings.profile_taa = 0.0f;
    assert(anti_aliasing_mode(settings) == AntiAliasingMode::Off);
    settings.profile_taa = 1.0f;
    assert(anti_aliasing_mode(settings) == AntiAliasingMode::Temporal);
    settings.profile_taa = 2.0f;
    assert(anti_aliasing_mode(settings) == AntiAliasingMode::TemporalSharp);
}

void global_quality_never_touches_ssao() {
    Settings settings = neutral();
    assert(ssao_quality(settings).samples == 16);
    assert(!ssao_quality(settings).half_resolution);
    settings.profile_global_quality = 2.0f;
    assert(ssao_quality(settings).samples == 16);
    assert(!ssao_quality(settings).half_resolution);
    assert(bloom_level_limit(settings) == 3);
    settings.profile_global_quality = 1.0f;
    assert(bloom_level_limit(settings) == 4);
    assert(ssao_quality(settings).samples == 16);
}

void only_the_ssao_page_drives_ssao_detail() {
    Settings settings = neutral();
    settings.profile_ssao_detail_quality = 1.0f;
    assert(ssao_quality(settings).samples == 12);
    settings.profile_ssao_detail_quality = 2.0f;
    assert(ssao_quality(settings).samples == 8);
    assert(ssao_quality(settings).half_resolution);
    settings.profile_ssao_detail_quality = 0.0f;
    assert(!ssao_quality(settings).half_resolution);
    settings.profile_use_half_res_ssao = 1.0f;
    assert(ssao_quality(settings).half_resolution);
}

void presets_widen_and_deepen() {
    Settings settings = neutral();
    const SsaoQuality soft = ssao_quality(settings);
    settings.profile_ssao_preset = 2.0f;
    const SsaoQuality strong = ssao_quality(settings);
    assert(strong.radius_scale > soft.radius_scale);
    assert(strong.bias_scale < soft.bias_scale);
    assert(strong.curve < soft.curve);
    assert(soft.radius_scale == 1.0f && soft.curve == 1.0f);
}

void switches_turn_effects_off() {
    Settings settings = neutral();
    assert(fxaa_enabled(settings));
    assert(std::fabs(interior_light_strength(settings) - 0.17f) < 1e-6f);
    assert(std::fabs(ssao_strength(settings) - 1.5f * kSsaoGain) < 1e-6f);
    settings.profile_fxaa = 0.0f;
    settings.profile_use_interior_lighting = 0.0f;
    settings.ssao_intensity_scale = 0.0f;
    assert(!fxaa_enabled(settings));
    assert(interior_light_strength(settings) == 0.0f);
    assert(ssao_strength(settings) == 0.0f);
}

}

int main() {
    levels_round_and_clamp();
    anti_aliasing_follows_the_list();
    global_quality_never_touches_ssao();
    only_the_ssao_page_drives_ssao_detail();
    presets_widen_and_deepen();
    switches_turn_effects_off();
    std::printf("effect_quality_test ok\n");
    return 0;
}
