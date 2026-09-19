#pragma once

#include "settings.hpp"

namespace photorealism {

enum class AntiAliasingMode {
    Off,
    Temporal,
    TemporalSharp,
};

constexpr float kLastSupportedAntiAliasing = 2.0f;
constexpr unsigned kMaximumSsaoSamples = 16;
constexpr float kClaritySharpness = 0.9f;
constexpr float kSsaoGain = 2.2f;
constexpr float kInteriorLightNearStart = 0.4f;
constexpr float kInteriorLightNearEnd = 1.0f;
constexpr float kInteriorLightGain = 3.0f;

struct SsaoQuality {
    unsigned samples;
    bool half_resolution;
    float radius_scale;
    float bias_scale;
    float curve;
};

struct PreToneParameters {
    float exposure_gain;
    float contrast;
};

unsigned quality_level(float value);
PreToneParameters pre_tone_parameters(const Settings& settings);
bool pre_tone_active(const PreToneParameters& parameters);
AntiAliasingMode anti_aliasing_mode(const Settings& settings);
SsaoQuality ssao_quality(const Settings& settings);
unsigned bloom_level_limit(const Settings& settings);
float ssao_strength(const Settings& settings);
bool fxaa_enabled(const Settings& settings);
bool shader_patch_active(const Settings& settings);
float interior_light_strength(const Settings& settings);

}
