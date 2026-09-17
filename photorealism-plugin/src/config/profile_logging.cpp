#include "profile_logging.hpp"

#include "../runtime.hpp"
#include "profile_fields.hpp"
#include "profile_state.hpp"

#include <cstdio>
#include <string>

namespace photorealism {
namespace {

void log_active_lighting(const Settings& settings) {
    const PhotorealismTonemap& tonemap = active_tonemap(settings);
    log_message(
        "Perfil photorealism 0.23.0: iluminacao %c (conjunto de tom %u) "
        "temperatura=%.0f exposicao=%.3f contraste=%.3f saturacao=%.3f "
        "vibracao=%.3f sombras=%.3f altas_luzes=%.3f pretos=%.3f brancos=%.3f "
        "exposicao_noturna=%+.2fEV nitidez=%.1f bordas=%.1f. Unica fonte do "
        "grade.",
        'A' + static_cast<char>(active_set_index(settings.profile_lighting_method)),
        active_set_index(settings.profile_lighting_method) + 1,
        tonemap.temperature, tonemap.exposure, tonemap.contrast,
        tonemap.saturation, tonemap.vibrance, tonemap.shadows,
        tonemap.highlights, tonemap.blacks, tonemap.whites,
        tonemap.night_exposure, settings.profile_sharpness,
        settings.profile_sharpen_edges);
}

void log_night_exposure(const Settings& settings) {
    if (settings.profile_night_exposure == 0.0f) {
        return;
    }
    log_message(
        settings.scene_observer_enabled
            ? "Perfil photorealism 0.23.0: exposicao noturna %+.2f EV somada a "
              "exposicao pelo peso de noite do detector."
            : "Perfil photorealism 0.23.0: exposicao noturna %+.2f EV sem "
              "efeito: o observador de cena precisa estar ligado para saber que "
              "e noite.",
        static_cast<double>(settings.profile_night_exposure));
}

void log_pre_tone(const Settings& settings) {
    const PhotorealismTonemap& tonemap = active_tonemap(settings);
    if (!uses_pre_tone_controls(tonemap)) {
        return;
    }
    log_message(
        "Perfil photorealism 0.23.0: pre_exposure=%.2f pre_contrast=%.2f "
        "dynamic_contrast=%.2f aplicados no HDR antes do tom do jogo.",
        static_cast<double>(tonemap.pre_exposure),
        static_cast<double>(tonemap.pre_contrast),
        static_cast<double>(tonemap.dynamic_contrast));
}

std::string keys_with_reason(const Settings& settings, PendingReason reason) {
    std::string keys;
    for (std::size_t index = 0; index < kPendingControlCount; ++index) {
        const PendingControl& pending = kPendingControls[index];
        if (pending.reason != reason) {
            continue;
        }
        char item[96] = {};
        std::snprintf(
            item, sizeof(item), " %s=%g", profile_key_for(pending.member),
            static_cast<double>(settings.*(pending.member)));
        keys.append(item);
    }
    return keys;
}

void log_pending_keys(const Settings& settings) {
    for (std::size_t index = 0; index < kPendingReasonCount; ++index) {
        const PendingReason reason = static_cast<PendingReason>(index);
        const std::string keys = keys_with_reason(settings, reason);
        if (keys.empty()) {
            continue;
        }
        log_message(
            "Perfil photorealism 0.23.0 sem efeito (%s):%s.",
            pending_reason_text(reason), keys.c_str());
    }
}

}

void log_profile(const Settings& settings) {
    log_active_lighting(settings);
    log_night_exposure(settings);
    log_pre_tone(settings);
    log_pending_keys(settings);
}

}
