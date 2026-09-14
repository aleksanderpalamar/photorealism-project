#include "condition_adapter.hpp"

#include "../runtime.hpp"

#include <windows.h>

namespace photorealism {

void ConditionAdapter::reset_log() {
    logged_once_ = false;
}

void ConditionAdapter::reset_state() {
    smoother_.reset();
}

void ConditionAdapter::update(
    const Settings& settings, const SceneFeatures& features) {
    if (!settings.condition_adaptation_enabled ||
        settings.condition_color_locked) {
        temperature_ = settings.temperature;
        tint_ = settings.tint;
        return;
    }

    ConditionThresholds thresholds = {};
    thresholds.daylight_median_low = settings.condition_daylight_median_low;
    thresholds.daylight_median_high =
        settings.condition_daylight_median_high;
    thresholds.overcast_saturation_low =
        settings.condition_overcast_saturation_low;
    thresholds.overcast_saturation_high =
        settings.condition_overcast_saturation_high;
    thresholds.minimum_dynamic_range =
        settings.condition_minimum_dynamic_range;

    const unsigned long long now = GetTickCount64();
    float elapsed = 0.0f;
    if (last_update_ms_ != 0ull && now > last_update_ms_) {
        elapsed =
            static_cast<float>(now - last_update_ms_) / 1000.0f;
    }
    last_update_ms_ = now;

    if (!smoother_.update(
            features,
            elapsed,
            settings.condition_time_constant_seconds,
            thresholds)) {
        temperature_ = settings.temperature;
        tint_ = settings.tint;
        return;
    }

    ConditionAnchors anchors = {};
    anchors.sun_temperature = settings.condition_sun_temperature;
    anchors.sun_tint = settings.condition_sun_tint;
    anchors.rain_temperature = settings.condition_rain_temperature;
    anchors.rain_tint = settings.condition_rain_tint;
    anchors.night_temperature = settings.condition_night_temperature;
    anchors.night_tint = settings.condition_night_tint;

    const ConditionWeights weights = compute_condition_weights(
        smoother_.median(),
        smoother_.saturation(),
        thresholds);
    const ConditionGrade grade = blend_condition_grade(weights, anchors);
    temperature_ = grade.temperature;
    tint_ = grade.tint;

    const float log_seconds = settings.condition_log_seconds;
    const bool due =
        log_seconds > 0.0f &&
        now - last_log_ms_ >=
            static_cast<unsigned long long>(log_seconds * 1000.0f);
    if (!logged_once_ || due) {
        log_message(
            "Condicao 0.19.0: sol=%.3f chuva=%.3f noite=%.3f "
            "(mediana=%.1f saturacao=%.3f suavizadas) -> "
            "temperature=%.0fK tint=%.3f.",
            static_cast<double>(weights.sun),
            static_cast<double>(weights.rain),
            static_cast<double>(weights.night),
            static_cast<double>(smoother_.median()),
            static_cast<double>(smoother_.saturation()),
            static_cast<double>(temperature_),
            static_cast<double>(tint_));
        last_log_ms_ = now;
        logged_once_ = true;
    }
}

}  // namespace photorealism
