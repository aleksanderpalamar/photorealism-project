#include "observer.hpp"

#include "../runtime.hpp"

#include <windows.h>

namespace photorealism {
namespace {
unsigned long long now_ms() {
    return static_cast<unsigned long long>(GetTickCount64());
}
}

void SceneObserver::configure(
    bool enabled, unsigned interval_frames, float log_seconds) {
    enabled_ = enabled;
    interval_frames_ = interval_frames < 1u ? 1u : interval_frames;
    log_seconds_ = log_seconds < 0.0f ? 0.0f : log_seconds;

    first_measurement_logged_ = false;
}

void SceneObserver::release() {
    sampler_.release();
    latest_ = SceneFeatures{};
}

void SceneObserver::observe(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* scene) {
    if (!enabled_ || device == nullptr || context == nullptr ||
        scene == nullptr) {
        return;
    }
    if (!sampler_.ensure_resources(device, scene)) {
        return;
    }
    if (sampler_.consume_creation_notice()) {
        const SceneSampler::Info info = sampler_.info();
        log_message(
            "Observador de cena 0.18.0 ativo: fonte %ux%u format=%u amostrado "
            "como %u, mip %u (%ux%u = %u pixels), intervalo=%u frames, "
            "log=%.0fs.",
            info.source_width,
            info.source_height,
            info.source_format,
            info.sample_format,
            info.mip_level,
            info.mip_width,
            info.mip_height,
            info.mip_width * info.mip_height,
            interval_frames_,
            static_cast<double>(log_seconds_));
    }

    sampler_.drain(context, this);

    ++frame_counter_;
    if (frame_counter_ < interval_frames_) {
        return;
    }
    frame_counter_ = 0u;
    sampler_.submit(context, scene);
}

void SceneObserver::on_sample(
    const unsigned char* pixels,
    unsigned width,
    unsigned height,
    unsigned pitch,
    bool bgra) {
    const SceneFeatures features =
        compute_scene_features(pixels, width, height, pitch, bgra);
    if (!features.valid) {
        return;
    }
    latest_ = features;

    const unsigned long long now = now_ms();
    const bool due =
        log_seconds_ > 0.0f &&
        now - last_log_ms_ >=
            static_cast<unsigned long long>(log_seconds_ * 1000.0f);
    if (!first_measurement_logged_ || due) {
        log_message(
            "Cena 0.18.0: ceu_R/B=%.3f mediana=%.1f faixa_p90-p10=%.1f "
            "saturacao=%.3f media=%.1f amostra=%u.",
            static_cast<double>(features.sky_r_over_b),
            static_cast<double>(features.median),
            static_cast<double>(features.dynamic_range),
            static_cast<double>(features.saturation),
            static_cast<double>(features.mean),
            features.sample_pixels);
        last_log_ms_ = now;
        first_measurement_logged_ = true;
    }
}

}
