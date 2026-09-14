#pragma once

#include "settings.hpp"

namespace photorealism {

Settings default_settings();
Settings default_settings_with_lighting(float lighting_method);
void finish_settings(Settings* settings);
bool read_settings(Settings* settings);
bool load_settings(Settings* settings);

}
