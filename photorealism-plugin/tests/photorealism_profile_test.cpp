#include <windows.h>

#include "../src/config/config.hpp"
#include "../src/config/defaults.hpp"
#include "../src/config/limits.hpp"
#include "../src/config/profile_layer.hpp"
#include "../src/config/profile_pending.hpp"
#include "../src/config/profile_switch.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

namespace photorealism {
namespace {
wchar_t g_config_path[4096] = {};
}

const wchar_t* config_path() { return g_config_path; }
void log_message(const char*, ...) {}
}

using namespace photorealism;

namespace {

const char* kTempPath = "/tmp/photorealism-profile-test.cfg";

bool near(float value, float target, float tolerance) {
    return std::fabs(value - target) <= tolerance;
}

Settings load_from_text(const char* text) {
    FILE* file = std::fopen(kTempPath, "wb");
    assert(file != nullptr);
    std::fwrite(text, 1, std::strlen(text), file);
    std::fclose(file);
    ::mbstowcs(g_config_path, kTempPath, 4095);
    Settings settings = {};
    assert(load_settings(&settings));
    std::remove(kTempPath);
    return settings;
}

void keys_land_in_the_numbered_set() {
    PhotorealismProfile profile;
    assert(apply_profile_key(&profile, "tonemap_exposure_2", "0.25"));
    assert(apply_profile_key(&profile, "tonemap_pre_contrast_2", "0.26"));
    assert(apply_profile_key(&profile, "tonemap_whites_5", "-0.13"));
    assert(apply_profile_key(&profile, "sharpen_edges", "4"));
    assert(near(profile.sets[1].exposure, 0.25f, 1e-6f));
    assert(near(profile.sets[1].pre_contrast, 0.26f, 1e-6f));
    assert(near(profile.sets[4].whites, -0.13f, 1e-6f));
    assert(near(active_tonemap(profile, 2.0f).exposure, 0.25f, 1e-6f));
    assert(uses_pending_controls(active_tonemap(profile, 2.0f)));
}

void malformed_keys_are_refused() {
    PhotorealismProfile profile;
    assert(!apply_profile_key(&profile, "tonemap_set", "2"));
    assert(!apply_profile_key(&profile, "tonemap_exposure_6", "1"));
    assert(!apply_profile_key(&profile, "tonemap_exposure", "1"));
    assert(!apply_profile_key(&profile, "tonemap_tonemap_1", "7"));
    assert(!apply_profile_key(&profile, "tonemap_operator_a", "7"));
    assert(!apply_profile_key(&profile, "lighting_method", "3"));
}

void the_set_number_is_rounded_and_bounded() {
    assert(tonemap_set_number(4.0f) == 4);
    assert(tonemap_set_number(2.6f) == 3);
    assert(tonemap_set_number(2.4f) == 2);
    assert(tonemap_set_number(0.0f) == 1);
    assert(tonemap_set_number(-7.0f) == 1);
    assert(tonemap_set_number(9.0f) == 5);
    assert(tonemap_set_number(1e30f) == 5);
    Settings settings = default_settings();
    settings.profile_tonemap_set = 9.0f;
    apply_limits(&settings);
    assert(settings.profile_tonemap_set == 5.0f);
}

void the_reference_set_becomes_the_base_layer() {
    const CalibrationLayer layer = profile_base_layer(
        reference_profile(), static_cast<float>(kReferenceTonemapSet));
    assert(layer.enabled);
    assert(near(layer.temperature, 6500.0f, 0.01f));
    assert(near(layer.exposure, -0.06f, 1e-6f));
    assert(near(layer.contrast, 0.99f, 1e-6f));
    assert(near(layer.saturation, 1.00f, 1e-6f));
    assert(near(layer.shadows, -0.01f, 1e-6f));
    assert(near(layer.highlights, -0.07f, 1e-6f));
    assert(near(layer.whites, -0.01f, 1e-6f));
    assert(near(layer.sharpness, 0.6f, 1e-6f));
    assert(near(layer.local_contrast, 0.4f, 1e-6f));
    assert(layer.vignette == 0.0f && layer.tint == 0.0f);
    assert(layer.highlight_rolloff == 0.0f && layer.black_lift_g == 0.0f);
    assert(!uses_pending_controls(
        active_tonemap(reference_profile(), static_cast<float>(kReferenceTonemapSet))));
}

void every_set_has_the_reference_values() {
    const PhotorealismProfile profile = reference_profile();
    const PhotorealismTonemap& first = profile.sets[0];
    assert(near(first.pre_exposure, -1.00f, 1e-6f));
    assert(near(first.pre_contrast, 0.42f, 1e-6f));
    assert(near(first.exposure, -0.09f, 1e-6f));
    assert(near(first.saturation, 0.82f, 1e-6f));
    assert(near(first.contrast, 0.92f, 1e-6f));
    assert(near(first.vibrance, -0.15f, 1e-6f));
    assert(near(first.whites, 0.03f, 1e-6f));
    assert(near(first.night_exposure, 2.00f, 1e-6f));
    const PhotorealismTonemap& second = profile.sets[1];
    assert(near(second.pre_contrast, 0.26f, 1e-6f));
    assert(near(second.exposure, 0.25f, 1e-6f));
    assert(near(second.saturation, 0.97f, 1e-6f));
    assert(near(second.contrast, 0.93f, 1e-6f));
    assert(near(second.shadows, -0.06f, 1e-6f));
    assert(near(second.highlights, 0.06f, 1e-6f));
    assert(near(second.blacks, -0.02f, 1e-6f));
    assert(near(second.whites, -0.13f, 1e-6f));
    const PhotorealismTonemap& third = profile.sets[2];
    assert(near(third.pre_exposure, 0.10f, 1e-6f));
    assert(near(third.saturation, 1.01f, 1e-6f));
    const PhotorealismTonemap& fifth = profile.sets[4];
    assert(fifth.exposure == 0.0f && fifth.saturation == 1.0f);
    assert(fifth.contrast == 1.0f && !uses_pending_controls(fifth));
}

std::string shipped_config_path() {
    const char* root = std::getenv("PHOTOREALISM_PROJECT_DIR");
    const std::string base = root != nullptr ? root : ".";
    return base + "/config/photorealism-plugin.cfg";
}

bool same_tonemap(const PhotorealismTonemap& left, const PhotorealismTonemap& right) {
    return near(left.temperature, right.temperature, 0.01f) &&
           near(left.pre_exposure, right.pre_exposure, 1e-6f) &&
           near(left.pre_contrast, right.pre_contrast, 1e-6f) &&
           near(left.dynamic_contrast, right.dynamic_contrast, 1e-6f) &&
           near(left.exposure, right.exposure, 1e-6f) &&
           near(left.saturation, right.saturation, 1e-6f) &&
           near(left.contrast, right.contrast, 1e-6f) &&
           near(left.vibrance, right.vibrance, 1e-6f) &&
           near(left.shadows, right.shadows, 1e-6f) &&
           near(left.highlights, right.highlights, 1e-6f) &&
           near(left.blacks, right.blacks, 1e-6f) &&
           near(left.whites, right.whites, 1e-6f) &&
           near(left.night_exposure, right.night_exposure, 1e-6f);
}

void the_shipped_cfg_and_the_code_agree_on_the_profile() {
    const std::string path = shipped_config_path();
    ::mbstowcs(g_config_path, path.c_str(), 4095);
    CalibrationStack shipped = {};
    assert(load_stack(&shipped));
    const CalibrationStack internal = reference_stack();
    for (unsigned index = 0; index < kProfileTonemapSets; ++index) {
        assert(same_tonemap(shipped.profile.sets[index], internal.profile.sets[index]));
    }
    assert(shipped.modules.profile_tonemap_set == internal.modules.profile_tonemap_set);
    assert(shipped.profile.sharpness == internal.profile.sharpness);
    assert(shipped.profile.sharpen_edges == internal.profile.sharpen_edges);
    assert(shipped.profile.ssao_intensity == internal.profile.ssao_intensity);
    for (std::size_t index = 0; index < kPendingProfileKeyCount; ++index) {
        const PendingProfileKey& pending = kPendingProfileKeys[index];
        assert(near(shipped.modules.*(pending.member), pending.reference, 1e-6f));
        assert(near(internal.modules.*(pending.member), pending.reference, 1e-6f));
    }
}

void the_chosen_set_drives_the_grade_and_the_night_exposure() {
    const Settings s = load_from_text(
        "[profile.photorealism.0.23.0]\n"
        "enabled=true\n"
        "tonemap_set=1\n"
        "[module.user.0.20.0]\n"
        "enabled=false\n");
    assert(near(s.exposure, -0.09f, 1e-6f));
    assert(near(s.saturation, 0.82f, 1e-6f));
    assert(near(s.contrast, 0.92f, 1e-6f));
    assert(near(s.vibrance, -0.15f, 1e-6f));
    assert(near(s.profile_night_exposure, 2.0f, 1e-6f));
    assert(near(s.profile_pre_exposure, -1.0f, 1e-6f));
    assert(near(s.profile_pre_contrast, 0.42f, 1e-6f));
    assert(s.profile_active_set == 1.0f);
    assert(near(night_adjusted_exposure(s, 0.0f), -0.09f, 1e-6f));
    assert(near(night_adjusted_exposure(s, 1.0f), 1.91f, 1e-5f));
    assert(near(night_adjusted_exposure(s, 0.5f), 0.91f, 1e-5f));
}

void pending_keys_are_read_without_touching_the_grade() {
    const Settings plain = load_from_text(
        "[profile.photorealism.0.23.0]\n"
        "enabled=true\n"
        "tonemap_set=4\n");
    const Settings pending = load_from_text(
        "[profile.photorealism.0.23.0]\n"
        "enabled=true\n"
        "tonemap_set=4\n"
        "lighting_interior=0.9\n"
        "surface_albedo_saturation=1.8\n"
        "tonemap_operator_a=2\n"
        "hide_show_key=35\n");
    assert(near(pending.profile_lighting_interior, 0.9f, 1e-6f));
    assert(near(pending.profile_surface_albedo_saturation, 1.8f, 1e-6f));
    assert(near(pending.profile_tonemap_operator_a, 2.0f, 1e-6f));
    assert(near(pending.profile_hide_show_key, 35.0f, 1e-6f));
    assert(pending.exposure == plain.exposure);
    assert(pending.saturation == plain.saturation);
    assert(pending.contrast == plain.contrast);
    assert(pending.sharpness == plain.sharpness);
    assert(pending.ssao_intensity_scale == plain.ssao_intensity_scale);
}

void the_switch_recomposes_only_when_the_choice_changes() {
    Settings live = load_from_text(
        "[profile.photorealism.0.23.0]\n"
        "enabled=true\n"
        "tonemap_set=4\n");
    assert(!profile_choice_changed(live));
    live.profile_tonemap_set = 4.3f;
    assert(!profile_choice_changed(live));
    live.profile_tonemap_set = 2.7f;
    assert(profile_choice_changed(live));
    live.photorealism_profile_enabled = false;
    assert(profile_choice_changed(live));
    live.profile_active_set = 0.0f;
    assert(!profile_choice_changed(live));
}

void the_profile_replaces_the_measured_layers_but_not_the_user() {
    const Settings s = load_from_text(
        "[profile.photorealism.0.23.0]\n"
        "enabled=true\n"
        "tonemap_set=4\n"
        "tonemap_exposure_4=-0.06\n"
        "ssao_intensity=1.5\n"
        "[base.0.1.2]\n"
        "enabled=true\n"
        "exposure=0.9\n"
        "[module.visual.0.2.0]\n"
        "enabled=true\n"
        "exposure_delta=0.5\n"
        "[module.user.0.20.0]\n"
        "enabled=true\n"
        "exposure_delta=0.02\n");
    assert(near(s.exposure, -0.04f, 1e-5f));
    assert(near(s.ssao_intensity_scale, 1.5f, 1e-6f));
    assert(s.condition_color_locked);
}

void without_the_profile_nothing_changes() {
    const Settings s = load_from_text(
        "[profile.photorealism.0.23.0]\n"
        "enabled=false\n"
        "tonemap_exposure_4=-0.06\n"
        "ssao_intensity=1.5\n"
        "[base.0.1.2]\n"
        "enabled=true\n"
        "exposure=0.9\n"
        "[module.visual.0.2.0]\n"
        "enabled=true\n"
        "exposure_delta=0.5\n"
        "[module.rain_overcast.0.3.0]\n"
        "enabled=false\n"
        "[module.user.0.20.0]\n"
        "enabled=false\n");
    assert(near(s.exposure, 1.4f, 1e-5f));
    assert(near(s.ssao_intensity_scale, 1.0f, 1e-6f));
    assert(!s.condition_color_locked);
    assert(s.profile_active_set == 0.0f);
    assert(s.profile_night_exposure == 0.0f);
    assert(s.profile_dynamic_contrast == 1.0f);
}

}

int main() {
    keys_land_in_the_numbered_set();
    malformed_keys_are_refused();
    the_set_number_is_rounded_and_bounded();
    the_reference_set_becomes_the_base_layer();
    every_set_has_the_reference_values();
    the_shipped_cfg_and_the_code_agree_on_the_profile();
    the_chosen_set_drives_the_grade_and_the_night_exposure();
    pending_keys_are_read_without_touching_the_grade();
    the_switch_recomposes_only_when_the_choice_changes();
    the_profile_replaces_the_measured_layers_but_not_the_user();
    without_the_profile_nothing_changes();
    std::printf("photorealism_profile_test ok\n");
    return 0;
}
