#pragma once

#include "../config/settings.hpp"

namespace photorealism {
namespace overlay {

struct SaveReport {
    bool written = false;
    int changed = 0;
};

SaveReport save_settings(const Settings& settings, const Settings& on_disk);

}
}
