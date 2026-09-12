#include "upscaler.hpp"

#include "../runtime.hpp"
#include "fsr_telemetry.hpp"

namespace photorealism {
namespace fsr {

Upscaler& upscaler() {
    static Upscaler instance;
    return instance;
}

void Upscaler::configure(const Settings& settings) {
    const bool was_pending = pending();
    requested_enabled_ = settings.fsr_enabled;
    requested_scale_ = clamp_scale(settings.fsr_render_scale);
    sharpness_ = settings.fsr_sharpness;
    if (!pending() || was_pending) {
        return;
    }
    log_message(
        "FSR pedido: %s escala=%.4f. So vale quando o jogo recriar o "
        "backbuffer -- troque a resolucao no ETS2 ou reinicie o jogo.",
        requested_enabled_ ? "ligado" : "desligado",
        static_cast<double>(requested_scale_));
}

void Upscaler::apply_requested() {
    const bool changed =
        enabled_ != requested_enabled_ || scale_ != requested_scale_;
    enabled_ = requested_enabled_;
    scale_ = requested_scale_;
    if (!changed) {
        return;
    }
    telemetry().reset();
    log_message(
        "FSR %s: escala=%.4f nitidez=%.2f.",
        enabled_ ? "ligado" : "desligado",
        static_cast<double>(scale_),
        static_cast<double>(sharpness_));
    if (!enabled_) {
        release();
    }
}

void Upscaler::release() {
    pipeline_.release();
    proxy_.release();
    extent_ = RenderExtent{};
}

ID3D11Texture2D* Upscaler::ensure_proxy(
    ID3D11Device* device, const D3D11_TEXTURE2D_DESC& back_buffer) {
    if (!enabled_ || device == nullptr) {
        return nullptr;
    }
    if (back_buffer.SampleDesc.Count != 1) {
        telemetry().record_skip("backbuffer com MSAA");
        return nullptr;
    }

    const RenderExtent internal =
        internal_extent(back_buffer.Width, back_buffer.Height, scale_);
    if (!extent_reduces_work(internal, back_buffer.Width, back_buffer.Height)) {
        telemetry().record_skip("escala nao reduz trabalho");
        return nullptr;
    }
    if (!proxy_.ensure(device, back_buffer, internal)) {
        telemetry().record_skip("textura interna indisponivel");
        return nullptr;
    }

    extent_ = internal;
    return proxy_.texture();
}

bool Upscaler::present(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* output,
    unsigned width,
    unsigned height) {
    if (!enabled_ || !proxy_.ready()) {
        return false;
    }
    if (!pipeline_.ensure(device, width, height)) {
        telemetry().record_skip("passe de upscale indisponivel");
        return false;
    }

    const bool ran = pipeline_.run(
        context, proxy_.view(), extent_, output, width, height, sharpness_);
    if (!ran) {
        telemetry().record_skip("passe de upscale recusou o quadro");
    }
    telemetry().report(extent_, width, height);
    return ran;
}

}
}
