#pragma once

#include "../config.hpp"
#include "../scene_conditions.hpp"
#include "../scene_features.hpp"

namespace photorealism {

class ConditionAdapter {
  public:
    void reset_log();
    void reset_state();
    void update(const Settings& settings, const SceneFeatures& features);

    float temperature() const { return temperature_; }
    float tint() const { return tint_; }

  private:
    ConditionSmoother smoother_;
    unsigned long long last_update_ms_ = 0ull;
    unsigned long long last_log_ms_ = 0ull;
    bool logged_once_ = false;
    float temperature_ = 6500.0f;
    float tint_ = 0.0f;
};

}  // namespace photorealism
