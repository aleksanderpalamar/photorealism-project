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
    static char line[96] = {};
    if (game_holds_proxy_ && extent_.width != 0) {
        std::snprintf(
            line, sizeof(line), "ATIVO  %ux%u reconstruido",
            extent_.width, extent_.height);
        return line;
    }
    if (enabled_) {
        return "LIGADO mas inativo -- reinicie o ETS2 para valer";
    }
    return "desligado";
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
        "FSR %s: escala=%.4f nitidez=%.2f. Vale quando o jogo pegar o "
        "backbuffer de novo.",
        enabled_ ? "ligado" : "desligado",
        static_cast<double>(scale_),
        static_cast<double>(sharpness_));
}

void Upscaler::release() {
    pipeline_.release();
    proxy_.release();
    extent_ = RenderExtent{};
    game_holds_proxy_ = false;
}

ID3D11Texture2D* Upscaler::proxy_for_plugin() const {
    return game_holds_proxy_ ? proxy_.texture() : nullptr;
}

ID3D11Texture2D* Upscaler::acquire_for_game(
    ID3D11Device* device, const D3D11_TEXTURE2D_DESC& back_buffer) {
    if (!enabled_ || device == nullptr) {
        game_holds_proxy_ = false;
        return nullptr;
    }
    if (back_buffer.SampleDesc.Count != 1) {
        telemetry().record_skip("backbuffer com MSAA");
        game_holds_proxy_ = false;
        return nullptr;
    }

    const RenderExtent internal =
        internal_extent(back_buffer.Width, back_buffer.Height, scale_);
    if (!extent_reduces_work(internal, back_buffer.Width, back_buffer.Height)) {
        telemetry().record_skip("escala nao reduz trabalho");
        game_holds_proxy_ = false;
        return nullptr;
    }
    if (!proxy_.ensure(device, back_buffer, internal)) {
        telemetry().record_skip("textura interna indisponivel");
        game_holds_proxy_ = false;
        return nullptr;
    }

    extent_ = internal;
    game_holds_proxy_ = true;
    return proxy_.texture();
}

bool Upscaler::present(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* output,
    unsigned width,
    unsigned height) {
    if (!enabled_ && !game_holds_proxy_) {
        return false;
    }
    if (!game_holds_proxy_ || !proxy_.ready()) {
        telemetry().record_skip("o jogo ainda nao pegou a textura interna");
        telemetry().report(extent_, width, height);
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
