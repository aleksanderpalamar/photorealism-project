#include "upscaler.hpp"

#include "../runtime.hpp"
#include "../resource_observer/color_observation.hpp"
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
    if (internal_.view() == nullptr) {
        return "LIGADO  procurando o quadro interno do jogo";
    }
    return "ATIVO  reconstruindo o quadro interno";
}

void Upscaler::configure(const Settings& settings) {
    const bool was_enabled = enabled_;
    enabled_ = settings.fsr_enabled;
    scale_ = clamp_scale(settings.fsr_render_scale);
    sharpness_ = settings.fsr_sharpness;
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
}

void Upscaler::release() {
    pipeline_.release();
    internal_.release();
    extent_ = RenderExtent{};
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
    set_color_search_window(width, height);
    if (!internal_.capture(device, context)) {
        telemetry().record_skip(
            "quadro interno do jogo ainda nao localizado");
        telemetry().report(extent_, width, height);
        return false;
    }
    extent_ = internal_.extent();
    if (!pipeline_.ensure(device, width, height)) {
        telemetry().record_skip("passe de upscale indisponivel");
        return false;
    }

    const bool ran = pipeline_.run(
        context, internal_.view(), extent_, output, width, height,
        sharpness_);
    if (!ran) {
        telemetry().record_skip("passe de upscale recusou o quadro");
    }
    telemetry().report(extent_, width, height);
    return ran;
}

}
}
