#pragma once

#include "calibration.hpp"
#include "settings.hpp"

namespace photorealism {

Settings default_settings();
bool load_settings(Settings* settings);
bool load_stack(CalibrationStack* stack);
Settings compose(const CalibrationStack& stack);

}
