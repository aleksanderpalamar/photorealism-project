#include "effect_quality.hpp"

namespace photorealism {
namespace {

constexpr unsigned kLevels = 3;
constexpr unsigned kSamplesByLevel[kLevels] = {16, 12, 8};
constexpr unsigned kBloomLevelsByQuality[kLevels] = {5, 4, 3};

struct SsaoPreset {
    float radius_scale;
    float bias_scale;
    float curve;
};

constexpr SsaoPreset kSsaoPresets[kLevels] = {
    {1.00f, 1.00f, 1.00f},
    {1.35f, 0.75f, 0.85f},
    {1.75f, 0.50f, 0.70f},
};

bool switched_on(float value) {
    return value >= 0.5f;
}

}

unsigned quality_level(float value) {
    const float rounded = value + 0.5f;
    if (!(rounded >= 1.0f)) {
        return 0;
    }
    if (rounded >= static_cast<float>(kLevels)) {
        return kLevels - 1;
    }
    return static_cast<unsigned>(rounded);
}

AntiAliasingMode anti_aliasing_mode(const Settings& settings) {
    const unsigned level = quality_level(settings.profile_taa);
    return static_cast<AntiAliasingMode>(level);
}

SsaoQuality ssao_quality(const Settings& settings) {
    const unsigned quality = quality_level(settings.profile_global_quality);
    const unsigned detail = quality_level(settings.profile_ssao_detail_quality);
    const unsigned coarsest = quality > detail ? quality : detail;
    const SsaoPreset& preset =
        kSsaoPresets[quality_level(settings.profile_ssao_preset)];
    SsaoQuality result = {};
    result.samples = kSamplesByLevel[coarsest];
    result.half_resolution =
        switched_on(settings.profile_use_half_res_ssao) || quality == kLevels - 1;
    result.radius_scale = preset.radius_scale;
    result.bias_scale = preset.bias_scale;
    result.curve = preset.curve;
    return result;
}

unsigned bloom_level_limit(const Settings& settings) {
    return kBloomLevelsByQuality[quality_level(settings.profile_global_quality)];
}

float ssao_strength(const Settings& settings) {
    return settings.ssao_intensity_scale * kSsaoGain;
}

bool fxaa_enabled(const Settings& settings) {
    return switched_on(settings.profile_fxaa);
}

float interior_light_strength(const Settings& settings) {
    if (!switched_on(settings.profile_use_interior_lighting)) {
        return 0.0f;
    }
    return settings.profile_lighting_interior;
}

}
