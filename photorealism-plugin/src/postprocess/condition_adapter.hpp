#pragma once

#include "../config/config.hpp"
#include "../scene/condition_model.hpp"
#include "../scene/condition_smoother.hpp"
#include "../scene/features.hpp"

namespace photorealism {

class ConditionAdapter {
  public:
    void reset_log();
    void reset_state();
    void update(const Settings& settings, const SceneFeatures& features);

    float night_weight() const { return night_weight_; }

  private:
    float elapsed_seconds(unsigned long long now);
    void log_when_due(
        const Settings& settings,
        const ConditionWeights& weights,
        unsigned long long now);

    ConditionSmoother smoother_;
    unsigned long long last_update_ms_ = 0ull;
    unsigned long long last_log_ms_ = 0ull;
    bool logged_once_ = false;
    float night_weight_ = 0.0f;
};

}
