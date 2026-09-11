#pragma once

#include "../config/calibration.hpp"
#include "../config/settings.hpp"

namespace photorealism {
namespace overlay {

struct SaveReport {
    bool written = false;
    int changed = 0;
};

Settings measured_baseline(const CalibrationStack& stack);

SaveReport save_settings(
    const Settings& settings,
    const Settings& baseline,
    const Settings& defaults);

}
}
