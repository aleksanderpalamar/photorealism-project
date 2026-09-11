#pragma once

#include "condition_model.hpp"
#include "features.hpp"

namespace photorealism {

class ConditionSmoother {
  public:
    void reset() {
        primed_ = false;
        median_ = 0.0f;
        saturation_ = 0.0f;
    }

    bool update(
        const SceneFeatures& features,
        float elapsed_seconds,
        float tau_seconds,
        const ConditionThresholds& thresholds) {
        if (!features.valid ||
            features.dynamic_range <= thresholds.minimum_dynamic_range) {
            return primed_;
        }
        if (!primed_) {
            median_ = features.median;
            saturation_ = features.saturation;
            primed_ = true;
            return true;
        }
        if (tau_seconds <= 0.0f) {
            median_ = features.median;
            saturation_ = features.saturation;
            return true;
        }
        if (elapsed_seconds <= 0.0f) {
            return true;
        }

        const float ratio = elapsed_seconds / tau_seconds;
        const float alpha =
            ratio >= 1.0f
                ? 1.0f
                : conditions_detail::clamp_unit(ratio * (1.0f - 0.5f * ratio));
        median_ += (features.median - median_) * alpha;
        saturation_ += (features.saturation - saturation_) * alpha;
        return true;
    }

    bool primed() const { return primed_; }
    float median() const { return median_; }
    float saturation() const { return saturation_; }

  private:
    bool primed_ = false;
    float median_ = 0.0f;
    float saturation_ = 0.0f;
};

}
