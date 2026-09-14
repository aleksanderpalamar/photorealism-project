#pragma once

#include "calibration.hpp"
#include "photorealism_profile.hpp"

namespace photorealism {

constexpr float kProfileSliderScale = 10.0f;

CalibrationLayer profile_base_layer(const PhotorealismProfile& profile);

}
