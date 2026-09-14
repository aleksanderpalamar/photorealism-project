#pragma once

#include "calibration.hpp"
#include "photorealism_profile.hpp"
#include "settings.hpp"

namespace photorealism {

constexpr float kProfileSliderScale = 10.0f;

CalibrationLayer profile_base_layer(
    const PhotorealismProfile& profile, float chosen_set);
void reset_profile_outputs(Settings* settings);
void apply_profile_outputs(Settings* settings, const PhotorealismProfile& profile);
void copy_profile_outputs(Settings* live, const Settings& composed);
float night_adjusted_exposure(const Settings& settings, float night_weight);

}
