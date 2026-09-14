#pragma once

#include "calibration.hpp"
#include "settings.hpp"

namespace photorealism {

void apply_layer_setting(
    CalibrationLayer* layer, const char* raw_key, const char* value);
void copy_base_layer(Settings* settings, const CalibrationLayer& layer);
void add_delta_layer(Settings* settings, const CalibrationLayer& layer);
void copy_grade_fields(Settings* settings, const Settings& source);

}
