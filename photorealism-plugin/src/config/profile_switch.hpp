#pragma once

#include "calibration.hpp"
#include "settings.hpp"

namespace photorealism {

void carry_profile_choice(CalibrationStack* stack, const Settings* live);
bool profile_choice_changed(const Settings& live);
bool switch_grade_profile(Settings* live);

}
