#include "profile_pending.hpp"

#include "text_utils.hpp"

#include <windows.h>

namespace photorealism {

const PendingProfileKey kPendingProfileKeys[] = {
    {"lighting_interior", &Settings::profile_lighting_interior, 0.17f, PendingReason::GameHdr},
    {"use_interior_lighting", &Settings::profile_use_interior_lighting, 1.0f, PendingReason::GameHdr},
    {"surface_albedo_saturation", &Settings::profile_surface_albedo_saturation, 1.07f, PendingReason::GBuffer},
    {"roads_normal_intensity", &Settings::profile_roads_normal_intensity, 3.0f, PendingReason::GBuffer},
    {"roads_default_normals", &Settings::profile_roads_default_normals, 1.0f, PendingReason::GBuffer},
    {"use_sss", &Settings::profile_use_sss, 1.0f, PendingReason::GBuffer},
    {"use_default_mirrors", &Settings::profile_use_default_mirrors, 0.0f, PendingReason::MirrorTarget},
    {"use_motion_blur", &Settings::profile_use_motion_blur, 1.0f, PendingReason::MotionVectors},
    {"motion_blur_intensity", &Settings::profile_motion_blur_intensity, 10.0f, PendingReason::MotionVectors},
    {"vegetation_leaves_thickness", &Settings::profile_vegetation_leaves_thickness, 0.0f, PendingReason::ShaderSwap},
    {"vegetation_grass_thickness", &Settings::profile_vegetation_grass_thickness, 1.0f, PendingReason::ShaderSwap},
    {"use_default_rain", &Settings::profile_use_default_rain, 0.0f, PendingReason::ShaderSwap},
    {"fxaa", &Settings::profile_fxaa, 1.0f, PendingReason::Fxaa},
    {"use_half_res_ssao", &Settings::profile_use_half_res_ssao, 0.0f, PendingReason::AlreadyMet},
    {"taa", &Settings::profile_taa, 4.0f, PendingReason::NoEquivalent},
    {"taa_level", &Settings::profile_taa_level, 1.0f, PendingReason::NoEquivalent},
    {"dlss_preset", &Settings::profile_dlss_preset, 2.0f, PendingReason::NoEquivalent},
    {"hide_show_key", &Settings::profile_hide_show_key, 520.0f, PendingReason::NoEquivalent},
    {"color_preset", &Settings::profile_color_preset, 0.0f, PendingReason::NoEquivalent},
    {"color_preset_extra_brightness", &Settings::profile_color_preset_extra_brightness, 0.0f, PendingReason::NoEquivalent},
    {"tonemap_operator", &Settings::profile_tonemap_operator, 0.0f, PendingReason::NoEquivalent},
    {"tonemap_operator_a", &Settings::profile_tonemap_operator_a, 7.0f, PendingReason::NoEquivalent},
    {"lighting_method", &Settings::profile_lighting_method, 3.0f, PendingReason::NoEquivalent},
    {"ssao_preset", &Settings::profile_ssao_preset, 0.0f, PendingReason::NoEquivalent},
    {"ssao_detail_quality", &Settings::profile_ssao_detail_quality, 0.0f, PendingReason::NoEquivalent},
    {"global_quality", &Settings::profile_global_quality, 0.0f, PendingReason::NoEquivalent},
};

const std::size_t kPendingProfileKeyCount =
    sizeof(kPendingProfileKeys) / sizeof(kPendingProfileKeys[0]);

const char* pending_reason_text(PendingReason reason) {
    constexpr const char* kTexts[kPendingReasonCount] = {
        "pendente: descoberta do G-buffer",
        "pendente: HDR do jogo",
        "pendente: vetores de movimento",
        "pendente: alvo do espelho",
        "exige troca de shader",
        "pendente: FXAA",
        "ja atendido: SSAO em resolucao cheia",
        "sem equivalente no plugin",
    };
    return kTexts[static_cast<std::size_t>(reason)];
}

bool apply_pending_key(Settings* modules, const char* key, const char* value) {
    for (std::size_t index = 0; index < kPendingProfileKeyCount; ++index) {
        const PendingProfileKey& pending = kPendingProfileKeys[index];
        if (_stricmp(pending.key, key) != 0) {
            continue;
        }
        modules->*(pending.member) = config_text::to_number(value);
        return true;
    }
    return false;
}

void apply_pending_references(Settings* modules) {
    for (std::size_t index = 0; index < kPendingProfileKeyCount; ++index) {
        const PendingProfileKey& pending = kPendingProfileKeys[index];
        modules->*(pending.member) = pending.reference;
    }
}

}
