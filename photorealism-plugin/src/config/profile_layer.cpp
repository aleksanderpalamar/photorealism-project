#include "profile_layer.hpp"

namespace photorealism {

CalibrationLayer profile_base_layer(
    const PhotorealismProfile& profile, float chosen_set) {
    const PhotorealismTonemap& tonemap = active_tonemap(profile, chosen_set);
    CalibrationLayer layer = {};
    layer.enabled = true;
    layer.temperature = tonemap.temperature;
    layer.exposure = tonemap.exposure;
    layer.contrast = tonemap.contrast;
    layer.saturation = tonemap.saturation;
    layer.vibrance = tonemap.vibrance;
    layer.shadows = tonemap.shadows;
    layer.highlights = tonemap.highlights;
    layer.blacks = tonemap.blacks;
    layer.whites = tonemap.whites;
    layer.sharpness = profile.sharpness / kProfileSliderScale;
    layer.local_contrast = profile.sharpen_edges / kProfileSliderScale;
    return layer;
}

void reset_profile_outputs(Settings* settings) {
    settings->ssao_intensity_scale = 1.0f;
    settings->condition_color_locked = false;
    settings->profile_active_set = 0.0f;
    settings->profile_night_exposure = 0.0f;
    settings->profile_pre_exposure = 0.0f;
    settings->profile_pre_contrast = 0.0f;
    settings->profile_dynamic_contrast = 1.0f;
}

void apply_profile_outputs(Settings* settings, const PhotorealismProfile& profile) {
    const PhotorealismTonemap& tonemap =
        active_tonemap(profile, settings->profile_tonemap_set);
    settings->ssao_intensity_scale = profile.ssao_intensity;
    settings->condition_color_locked = true;
    settings->profile_active_set =
        static_cast<float>(tonemap_set_number(settings->profile_tonemap_set));
    settings->profile_night_exposure = tonemap.night_exposure;
    settings->profile_pre_exposure = tonemap.pre_exposure;
    settings->profile_pre_contrast = tonemap.pre_contrast;
    settings->profile_dynamic_contrast = tonemap.dynamic_contrast;
}

void copy_profile_outputs(Settings* live, const Settings& composed) {
    live->ssao_intensity_scale = composed.ssao_intensity_scale;
    live->condition_color_locked = composed.condition_color_locked;
    live->profile_active_set = composed.profile_active_set;
    live->profile_night_exposure = composed.profile_night_exposure;
    live->profile_pre_exposure = composed.profile_pre_exposure;
    live->profile_pre_contrast = composed.profile_pre_contrast;
    live->profile_dynamic_contrast = composed.profile_dynamic_contrast;
}

float night_adjusted_exposure(const Settings& settings, float night_weight) {
    return settings.exposure + settings.profile_night_exposure * night_weight;
}

}
