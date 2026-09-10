#pragma once

#include "scene_features.hpp"

namespace photorealism {
struct ConditionThresholds {
    float daylight_median_low;
    float daylight_median_high;

    float overcast_saturation_low;
    float overcast_saturation_high;

    float minimum_dynamic_range;
};

struct ConditionAnchors {
    float sun_temperature;
    float sun_tint;
    float rain_temperature;
    float rain_tint;
    float night_temperature;
    float night_tint;
};

struct ConditionWeights {
    float sun;
    float rain;
    float night;
};

struct ConditionGrade {
    float temperature;
    float tint;
};

namespace conditions_detail {
inline float clamp_unit(float value) {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

inline float smoothstep_value(float edge0, float edge1, float x) {
    if (edge1 <= edge0) {
        return x < edge0 ? 0.0f : 1.0f;
    }
    const float t = clamp_unit((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}
}

inline ConditionThresholds default_condition_thresholds() {
    ConditionThresholds t = {};

    t.daylight_median_low = 3.0f;
    t.daylight_median_high = 30.0f;
    t.overcast_saturation_low = 0.09f;
    t.overcast_saturation_high = 0.18f;
    t.minimum_dynamic_range = 20.0f;
    return t;
}

inline ConditionAnchors default_condition_anchors() {
    ConditionAnchors a = {};
    a.sun_temperature = 5900.0f;
    a.sun_tint = 0.44f;
    a.rain_temperature = 7500.0f;
    a.rain_tint = 0.30f;
    a.night_temperature = 7000.0f;
    a.night_tint = 0.34f;
    return a;
}

inline ConditionWeights compute_condition_weights(
    float median, float saturation, const ConditionThresholds& t) {
    const float daylight = conditions_detail::smoothstep_value(
        t.daylight_median_low, t.daylight_median_high, median);

    const float overcast = 1.0f - conditions_detail::smoothstep_value(
        t.overcast_saturation_low, t.overcast_saturation_high, saturation);
    ConditionWeights w = {};
    w.night = 1.0f - daylight;
    w.rain = daylight * overcast;
    w.sun = daylight * (1.0f - overcast);
    return w;
}

inline ConditionGrade blend_condition_grade(
    const ConditionWeights& w, const ConditionAnchors& a) {
    ConditionGrade g = {};
    g.temperature = w.sun * a.sun_temperature + w.rain * a.rain_temperature +
                    w.night * a.night_temperature;
    g.tint = w.sun * a.sun_tint + w.rain * a.rain_tint + w.night * a.night_tint;
    return g;
}

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
        float alpha = 1.0f;
        if (tau_seconds > 0.0f && elapsed_seconds > 0.0f) {
            const float ratio = elapsed_seconds / tau_seconds;
            alpha = ratio >= 1.0f ? 1.0f : ratio * (1.0f - 0.5f * ratio);
            alpha = conditions_detail::clamp_unit(alpha);
        }
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
