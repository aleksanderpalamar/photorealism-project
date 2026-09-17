#pragma once

#include "settings.hpp"

namespace photorealism {

constexpr float kProfileSliderScale = 10.0f;

PhotorealismTonemap& active_tonemap(Settings* settings);
const PhotorealismTonemap& active_tonemap(const Settings& settings);
void apply_active_tonemap(Settings* settings);
void store_active_tonemap(Settings* settings);
void derive_profile_controls(Settings* settings);
float night_adjusted_exposure(const Settings& settings, float night_weight);

}
