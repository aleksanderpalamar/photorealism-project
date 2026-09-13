#include "upscaler.hpp"

#include "../resource_observer/color_observation.hpp"
#include "../runtime.hpp"
#include "fsr_telemetry.hpp"
#include "output_target.hpp"

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
        line, sizeof(line), "ATIVO  %ux%u reconstruido antes da interface",
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
    reconstructed_this_frame_ = false;
}

bool Upscaler::reconstruct(
    ID3D11DeviceContext* context, ID3D11RenderTargetView* output) {
    OutputTarget target;
    if (!enabled_ || !describe_output(output, &target)) {
        return false;
    }
    if (!pipeline_.ready_for(target.width, target.height)) {
        return false;
    }
    if (!internal_.acquire()) {
        return false;
    }
    extent_ = internal_.extent();
    const bool ran = pipeline_.run(
        context, internal_.view(), extent_, output, target.width,
        target.height, sharpness_, target.srgb_view);
    internal_.release();
    if (!ran) {
        return false;
    }
    reconstructed_this_frame_ = true;
    telemetry().record_replacement();
    if (!placement_logged_) {
        log_message(
            "FSR reconstroi %ux%u para %ux%u na segunda passagem do jogo pelo "
            "backbuffer: depois do upscale do proprio jogo e antes da interface, "
            "que continua sendo desenhada por cima. RTV sRGB=%s.",
            extent_.width, extent_.height, target.width, target.height,
            target.srgb_view ? "sim" : "nao");
        placement_logged_ = true;
    }
    return true;
}

const char* Upscaler::skip_reason() const {
    if (!color_frame_captured()) {
        return "o jogo nao passou da resolucao interna para a de saida";
    }
    if (!pipeline_.ready()) {
        return "passe de upscale indisponivel";
    }
    return "o jogo nao voltou ao backbuffer depois de esticar o quadro interno";
}

void Upscaler::account_frame() {
    reconstructing_ = reconstructed_this_frame_;
    if (reconstructed_this_frame_) {
        reconstructed_this_frame_ = false;
        return;
    }
    telemetry().record_skip(skip_reason());
}

void Upscaler::present(
    ID3D11Device* device,
    ID3D11Texture2D* back_buffer,
    unsigned width,
    unsigned height) {
    if (!enabled_) {
        return;
    }
    const RenderExtent expected = internal_extent(width, height, capture_scale_);
    if (!extent_reduces_work(expected, width, height)) {
        reconstructing_ = false;
        telemetry().record_skip("a escala de inicio nao reduz trabalho");
        telemetry().report(extent_, width, height);
        return;
    }
    account_frame();
    if (color_frame_captured()) {
        pipeline_.ensure(device, width, height);
    }
    enable_color_capture(
        back_buffer, width, height, expected.width, expected.height);
    telemetry().report(extent_, width, height);
}

}
}
