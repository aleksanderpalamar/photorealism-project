#pragma once

#include "../config/settings.hpp"

namespace photorealism {

class EffectLog {
  public:
    void report(const Settings& settings);

  private:
    char last_[512] = {};
    unsigned long long last_tick_ = 0ull;
};

}
