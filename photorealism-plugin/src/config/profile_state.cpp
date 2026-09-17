#include "profile_state.hpp"

namespace photorealism {
namespace {

struct ToneLink {
    float Settings::*live;
    float PhotorealismTonemap::*stored;
};

constexpr ToneLink kToneLinks[] = {
    {&Settings::temperature, &PhotorealismTonemap::temperature},
    {&Settings::profile_pre_exposure, &PhotorealismTonemap::pre_exposure},
    {&Settings::profile_pre_contrast, &PhotorealismTonemap::pre_contrast},
    {&Settings::profile_dynamic_contrast, &PhotorealismTonemap::dynamic_contrast},
    {&Settings::exposure, &PhotorealismTonemap::exposure},
    {&Settings::saturation, &PhotorealismTonemap::saturation},
    {&Settings::contrast, &PhotorealismTonemap::contrast},
    {&Settings::vibrance, &PhotorealismTonemap::vibrance},
    {&Settings::shadows, &PhotorealismTonemap::shadows},
    {&Settings::highlights, &PhotorealismTonemap::highlights},
    {&Settings::blacks, &PhotorealismTonemap::blacks},
    {&Settings::whites, &PhotorealismTonemap::whites},
    {&Settings::profile_night_exposure, &PhotorealismTonemap::night_exposure},
};

}

PhotorealismTonemap& active_tonemap(Settings* settings) {
    const unsigned index = active_set_index(settings->profile_lighting_method);
    return settings->tonemap_sets[index];
}

const PhotorealismTonemap& active_tonemap(const Settings& settings) {
    const unsigned index = active_set_index(settings.profile_lighting_method);
    return settings.tonemap_sets[index];
}

void apply_active_tonemap(Settings* settings) {
    const PhotorealismTonemap& tonemap = active_tonemap(*settings);
    for (const ToneLink& link : kToneLinks) {
        settings->*(link.live) = tonemap.*(link.stored);
    }
}

void store_active_tonemap(Settings* settings) {
    PhotorealismTonemap& tonemap = active_tonemap(settings);
    for (const ToneLink& link : kToneLinks) {
        tonemap.*(link.stored) = settings->*(link.live);
    }
}

void derive_profile_controls(Settings* settings) {
    settings->sharpness = settings->profile_sharpness / kProfileSliderScale;
    settings->local_contrast =
        settings->profile_sharpen_edges / kProfileSliderScale;
    settings->ssao_intensity_scale = settings->profile_ssao_intensity;
    settings->temporal_enabled = settings->profile_taa >= 0.5f;
}

float night_adjusted_exposure(const Settings& settings, float night_weight) {
    return settings.exposure + settings.profile_night_exposure * night_weight;
}

}
