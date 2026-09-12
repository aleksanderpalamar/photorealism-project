#include "upscaler.hpp"

#include "../resource_observer/color_observation.hpp"
#include "../runtime.hpp"
#include "fsr_telemetry.hpp"

#include <cstdio>

namespace photorealism {
namespace fsr {

Upscaler& upscaler() {
    static Upscaler instance;
    return instance;
}

const char* Upscaler::status() const {
    static char line[96] = {};
    if (!enabled_) {
        return "desligado -- o ETS2 desenha em resolucao cheia";
    }
    if (!reconstructing_) {
        return "LIGADO  esperando o quadro interno -- reinicie se acabou de ligar";
    }
    std::snprintf(
        line, sizeof(line), "ATIVO  %ux%u reconstruido para a saida",
        extent_.width, extent_.height);
    return line;
}

void Upscaler::configure(const Settings& settings) {
    const bool was_enabled = enabled_;
    enabled_ = settings.fsr_enabled;
    scale_ = clamp_scale(settings.fsr_render_scale);
    sharpness_ = settings.fsr_sharpness;
    if (!capture_scale_known_) {
        capture_scale_ = scale_;
        capture_scale_known_ = true;
    }
    if (was_enabled == enabled_) {
        return;
    }
    telemetry().reset();
    log_message(
        "FSR %s: escala=%.4f nitidez=%.2f. A escala vale no config do jogo, "
        "aplicada no proximo inicio.",
        enabled_ ? "ligado" : "desligado",
        static_cast<double>(scale_),
        static_cast<double>(sharpness_));
    if (!enabled_) {
        disable_color_capture();
        release();
    }
}

void Upscaler::release() {
    pipeline_.release();
    internal_.release();
    reconstructing_ = false;
}

bool Upscaler::reconstruct(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* output,
    unsigned width,
    unsigned height,
    bool output_is_srgb_view) {
    if (!internal_.acquire()) {
        telemetry().record_skip(
            "o jogo nao passou da resolucao interna para a de saida");
        return false;
    }
    extent_ = internal_.extent();
    const bool ran =
        pipeline_.ensure(device, width, height) &&
        pipeline_.run(
            context, internal_.view(), extent_, output, width, height,
            sharpness_, output_is_srgb_view);
    internal_.release();
    if (!ran) {
        telemetry().record_skip("passe de upscale indisponivel");
        return false;
    }
    telemetry().record_replacement();
    return true;
}

bool Upscaler::present(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* output,
    unsigned width,
    unsigned height,
    bool output_is_srgb_view) {
    if (!enabled_) {
        return false;
    }
    const RenderExtent expected = internal_extent(width, height, capture_scale_);
    if (!extent_reduces_work(expected, width, height)) {
        reconstructing_ = false;
        telemetry().record_skip("a escala de inicio nao reduz trabalho");
        telemetry().report(extent_, width, height);
        return false;
    }
    enable_color_capture(width, height, expected.width, expected.height);
    reconstructing_ = reconstruct(
        device, context, output, width, height, output_is_srgb_view);
    telemetry().report(extent_, width, height);
    return reconstructing_;
}

}
}
