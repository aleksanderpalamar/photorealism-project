#include "upscaler.hpp"

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
    if (!enabled_) {
        return "desligado -- o ETS2 desenha em resolucao cheia";
    }
    if (internal_frame_ == nullptr) {
        return "LIGADO  o ETS2 desenha reduzido; reconstrucao propria pendente";
    }
    return "ATIVO  reconstruindo o quadro interno";
}

void Upscaler::configure(const Settings& settings) {
    const bool was_enabled = enabled_;
    const float was_scale = scale_;
    enabled_ = settings.fsr_enabled;
    scale_ = clamp_scale(settings.fsr_render_scale);
    sharpness_ = settings.fsr_sharpness;
    if (was_enabled == enabled_ && was_scale == scale_) {
        return;
    }
    telemetry().reset();
    log_message(
        "FSR %s: escala=%.4f nitidez=%.2f. A escala vale no config do jogo, "
        "aplicada no proximo inicio.",
        enabled_ ? "ligado" : "desligado",
        static_cast<double>(scale_),
        static_cast<double>(sharpness_));
}

void Upscaler::release() {
    pipeline_.release();
    extent_ = RenderExtent{};
    internal_frame_ = nullptr;
}

bool Upscaler::present(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* output,
    unsigned width,
    unsigned height) {
    if (!enabled_) {
        return false;
    }
    if (internal_frame_ == nullptr) {
        telemetry().record_skip(
            "quadro interno do jogo ainda nao localizado");
        telemetry().report(extent_, width, height);
        return false;
    }
    if (!pipeline_.ensure(device, width, height)) {
        telemetry().record_skip("passe de upscale indisponivel");
        return false;
    }

    const bool ran = pipeline_.run(
        context, internal_frame_, extent_, output, width, height, sharpness_);
    if (!ran) {
        telemetry().record_skip("passe de upscale recusou o quadro");
    }
    telemetry().report(extent_, width, height);
    return ran;
}

}
}
