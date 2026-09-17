#include "condition_adapter.hpp"

#include "../runtime.hpp"

#include <windows.h>

namespace photorealism {
namespace {

ConditionThresholds thresholds_from(const Settings& settings) {
    ConditionThresholds thresholds = {};
    thresholds.daylight_median_low = settings.condition_daylight_median_low;
    thresholds.daylight_median_high = settings.condition_daylight_median_high;
    thresholds.overcast_saturation_low =
        settings.condition_overcast_saturation_low;
    thresholds.overcast_saturation_high =
        settings.condition_overcast_saturation_high;
    thresholds.minimum_dynamic_range = settings.condition_minimum_dynamic_range;
    return thresholds;
}

}

void ConditionAdapter::reset_log() {
    logged_once_ = false;
}

void ConditionAdapter::reset_state() {
    smoother_.reset();
    night_weight_ = 0.0f;
}

float ConditionAdapter::elapsed_seconds(unsigned long long now) {
    const float elapsed =
        last_update_ms_ != 0ull && now > last_update_ms_
            ? static_cast<float>(now - last_update_ms_) / 1000.0f
            : 0.0f;
    last_update_ms_ = now;
    return elapsed;
}

void ConditionAdapter::update(
    const Settings& settings, const SceneFeatures& features) {
    const ConditionThresholds thresholds = thresholds_from(settings);
    const unsigned long long now = GetTickCount64();
    if (!smoother_.update(
            features,
            elapsed_seconds(now),
            settings.condition_time_constant_seconds,
            thresholds)) {
        return;
    }

    const ConditionWeights weights = compute_condition_weights(
        smoother_.median(),
        smoother_.saturation(),
        thresholds);
    night_weight_ = weights.night;
    log_when_due(settings, weights, now);
}

void ConditionAdapter::log_when_due(
    const Settings& settings,
    const ConditionWeights& weights,
    unsigned long long now) {
    const float log_seconds = settings.condition_log_seconds;
    const bool due =
        log_seconds > 0.0f &&
        now - last_log_ms_ >=
            static_cast<unsigned long long>(log_seconds * 1000.0f);
    if (logged_once_ && !due) {
        return;
    }
    log_message(
        "Condicao 0.19.0: sol=%.3f chuva=%.3f noite=%.3f "
        "(mediana=%.1f saturacao=%.3f suavizadas) -> "
        "exposicao_noturna=%+.2fEV.",
        static_cast<double>(weights.sun),
        static_cast<double>(weights.rain),
        static_cast<double>(weights.night),
        static_cast<double>(smoother_.median()),
        static_cast<double>(smoother_.saturation()),
        static_cast<double>(settings.profile_night_exposure * night_weight_));
    last_log_ms_ = now;
    logged_once_ = true;
}

}
