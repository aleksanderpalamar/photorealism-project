#include "profile_logging.hpp"

#include "../runtime.hpp"
#include "profile_pending.hpp"

#include <cstdio>
#include <string>

namespace photorealism {
namespace {

void log_active_set(const CalibrationStack& stack, const Settings& settings) {
    const PhotorealismTonemap& tonemap =
        active_tonemap(stack.profile, settings.profile_tonemap_set);
    log_message(
        "Perfil photorealism 0.23.0: ativo conjunto=%u de %u temperatura=%.0f "
        "exposicao=%.3f contraste=%.3f saturacao=%.3f vibracao=%.3f "
        "sombras=%.3f altas_luzes=%.3f pretos=%.3f brancos=%.3f "
        "exposicao_noturna=%+.2fEV nitidez=%.1f bordas=%.1f ssao=x%.2f. "
        "Camadas medidas fora da composicao; cor sem adaptacao por condicao.",
        tonemap_set_number(settings.profile_tonemap_set), kProfileTonemapSets,
        tonemap.temperature, tonemap.exposure, tonemap.contrast,
        tonemap.saturation, tonemap.vibrance, tonemap.shadows,
        tonemap.highlights, tonemap.blacks, tonemap.whites,
        tonemap.night_exposure, stack.profile.sharpness,
        stack.profile.sharpen_edges, stack.profile.ssao_intensity);
}

void log_night_exposure(const Settings& settings) {
    if (settings.profile_night_exposure == 0.0f) {
        return;
    }
    const bool knows_night =
        settings.condition_adaptation_enabled && settings.scene_observer_enabled;
    log_message(
        knows_night
            ? "Perfil photorealism 0.23.0: exposicao noturna %+.2f EV somada a "
              "exposicao pelo peso de noite da adaptacao por condicao."
            : "Perfil photorealism 0.23.0: exposicao noturna %+.2f EV sem "
              "efeito: o observador de cena e a adaptacao por condicao precisam "
              "estar ligados para saber que e noite.",
        static_cast<double>(settings.profile_night_exposure));
}

void log_pending_controls(const CalibrationStack& stack, const Settings& settings) {
    const PhotorealismTonemap& tonemap =
        active_tonemap(stack.profile, settings.profile_tonemap_set);
    if (!uses_pending_controls(tonemap)) {
        return;
    }
    log_message(
        "Perfil photorealism 0.23.0: conjunto %u com pre_exposure=%.2f "
        "pre_contrast=%.2f dynamic_contrast=%.2f; lidos, %s.",
        tonemap_set_number(settings.profile_tonemap_set),
        static_cast<double>(tonemap.pre_exposure),
        static_cast<double>(tonemap.pre_contrast),
        static_cast<double>(tonemap.dynamic_contrast),
        pending_reason_text(PendingReason::GameHdr));
}

std::string keys_with_reason(const Settings& settings, PendingReason reason) {
    std::string keys;
    for (std::size_t index = 0; index < kPendingProfileKeyCount; ++index) {
        const PendingProfileKey& pending = kPendingProfileKeys[index];
        if (pending.reason != reason) {
            continue;
        }
        char item[96] = {};
        std::snprintf(
            item, sizeof(item), " %s=%g", pending.key,
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

void log_profile(const CalibrationStack& stack, const Settings& settings) {
    log_pending_keys(settings);
    if (!settings.photorealism_profile_enabled) {
        log_message(
            "Perfil photorealism 0.23.0: inativo; o grade vem das camadas "
            "medidas.");
        return;
    }
    log_active_set(stack, settings);
    log_night_exposure(settings);
    log_pending_controls(stack, settings);
}

}
