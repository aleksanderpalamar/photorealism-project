#include "profile_fields.hpp"

namespace photorealism {

constexpr ModuleField kProfileFields[kProfileFieldCount] = {
    {"lighting_method", &Settings::profile_lighting_method},
    {"global_quality", &Settings::profile_global_quality},
    {"taa", &Settings::profile_taa},
    {"taa_level", &Settings::profile_taa_level},
    {"dlss_preset", &Settings::profile_dlss_preset},
    {"fxaa", &Settings::profile_fxaa},
    {"sharpness", &Settings::profile_sharpness},
    {"sharpen_edges", &Settings::profile_sharpen_edges},
    {"use_motion_blur", &Settings::profile_use_motion_blur},
    {"motion_blur_intensity", &Settings::profile_motion_blur_intensity},
    {"ssao_intensity", &Settings::profile_ssao_intensity},
    {"use_half_res_ssao", &Settings::profile_use_half_res_ssao},
    {"ssao_preset", &Settings::profile_ssao_preset},
    {"ssao_detail_quality", &Settings::profile_ssao_detail_quality},
    {"lighting_interior", &Settings::profile_lighting_interior},
    {"use_interior_lighting", &Settings::profile_use_interior_lighting},
    {"use_default_mirrors", &Settings::profile_use_default_mirrors},
    {"use_default_rain", &Settings::profile_use_default_rain},
    {"use_sss", &Settings::profile_use_sss},
    {"surface_albedo_saturation", &Settings::profile_surface_albedo_saturation},
    {"roads_normal_intensity", &Settings::profile_roads_normal_intensity},
    {"roads_default_normals", &Settings::profile_roads_default_normals},
    {"vegetation_leaves_thickness", &Settings::profile_vegetation_leaves_thickness},
    {"vegetation_grass_thickness", &Settings::profile_vegetation_grass_thickness},
    {"color_preset", &Settings::profile_color_preset},
    {"color_preset_extra_brightness", &Settings::profile_color_preset_extra_brightness},
    {"tonemap_operator", &Settings::profile_tonemap_operator},
    {"tonemap_operator_a", &Settings::profile_tonemap_operator_a},
    {"hide_show_key", &Settings::profile_hide_show_key},
};

static_assert(kProfileFields[kProfileFieldCount - 1].key != nullptr);

const PendingControl kPendingControls[] = {
    {&Settings::profile_taa_level, PendingReason::NoEquivalent},
    {&Settings::profile_dlss_preset, PendingReason::NoEquivalent},
    {&Settings::profile_use_motion_blur, PendingReason::MotionVectors},
    {&Settings::profile_motion_blur_intensity, PendingReason::MotionVectors},
    {&Settings::profile_use_default_mirrors, PendingReason::MirrorTarget},
    {&Settings::profile_use_default_rain, PendingReason::ShaderSwap},
    {&Settings::profile_use_sss, PendingReason::GBuffer},
    {&Settings::profile_surface_albedo_saturation, PendingReason::GBuffer},
    {&Settings::profile_roads_normal_intensity, PendingReason::GBuffer},
    {&Settings::profile_roads_default_normals, PendingReason::GBuffer},
    {&Settings::profile_vegetation_leaves_thickness, PendingReason::ShaderSwap},
    {&Settings::profile_vegetation_grass_thickness, PendingReason::ShaderSwap},
    {&Settings::profile_color_preset, PendingReason::NoEquivalent},
    {&Settings::profile_color_preset_extra_brightness, PendingReason::NoEquivalent},
    {&Settings::profile_tonemap_operator, PendingReason::NoEquivalent},
    {&Settings::profile_tonemap_operator_a, PendingReason::NoEquivalent},
    {&Settings::profile_hide_show_key, PendingReason::NoEquivalent},
};

const std::size_t kPendingControlCount =
    sizeof(kPendingControls) / sizeof(kPendingControls[0]);

const char* pending_reason_text(PendingReason reason) {
    constexpr const char* kTexts[kPendingReasonCount] = {
        "pendente: descoberta do G-buffer",
        "pendente: HDR do jogo",
        "pendente: vetores de movimento",
        "pendente: alvo do espelho",
        "exige troca de shader",
        "sem equivalente no plugin",
    };
    return kTexts[static_cast<std::size_t>(reason)];
}

const char* profile_key_for(float Settings::*member) {
    for (std::size_t index = 0; index < kProfileFieldCount; ++index) {
        if (kProfileFields[index].member == member) {
            return kProfileFields[index].key;
        }
    }
    return nullptr;
}

}
