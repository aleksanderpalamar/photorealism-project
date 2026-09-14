#include <windows.h>

#include "../src/config/config.hpp"
#include "../src/config/defaults.hpp"
#include "../src/config/profile_fields.hpp"
#include "../src/config/profile_state.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

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
    PhotorealismTonemap sets[kProfileTonemapSets];
    assert(apply_tonemap_key(sets, "tonemap_exposure_2", "0.25"));
    assert(apply_tonemap_key(sets, "tonemap_pre_contrast_2", "0.26"));
    assert(apply_tonemap_key(sets, "tonemap_whites_5", "-0.13"));
    assert(near(sets[1].exposure, 0.25f, 1e-6f));
    assert(near(sets[1].pre_contrast, 0.26f, 1e-6f));
    assert(near(sets[4].whites, -0.13f, 1e-6f));
    assert(uses_pending_controls(sets[1]));
    assert(!apply_tonemap_key(sets, "tonemap_exposure_6", "1"));
    assert(!apply_tonemap_key(sets, "tonemap_exposure", "1"));
    assert(!apply_tonemap_key(sets, "tonemap_operator_a", "7"));
    assert(!apply_tonemap_key(sets, "lighting_method", "3"));
}

void the_lighting_method_picks_the_set() {
    assert(active_set_index(0.0f) == 0);
    assert(active_set_index(1.0f) == 1);
    assert(active_set_index(2.6f) == 3);
    assert(active_set_index(3.0f) == 3);
    assert(active_set_index(-5.0f) == 0);
    assert(active_set_index(1e30f) == 3);
}

void every_set_has_the_reference_values() {
    PhotorealismTonemap sets[kProfileTonemapSets];
    reference_tonemap_sets(sets);
    assert(near(sets[0].pre_exposure, -1.00f, 1e-6f));
    assert(near(sets[0].pre_contrast, 0.42f, 1e-6f));
    assert(near(sets[0].exposure, -0.09f, 1e-6f));
    assert(near(sets[0].saturation, 0.82f, 1e-6f));
    assert(near(sets[0].contrast, 0.92f, 1e-6f));
    assert(near(sets[0].vibrance, -0.15f, 1e-6f));
    assert(near(sets[0].whites, 0.03f, 1e-6f));
    assert(near(sets[0].night_exposure, 2.00f, 1e-6f));
    assert(near(sets[1].exposure, 0.25f, 1e-6f));
    assert(near(sets[1].shadows, -0.06f, 1e-6f));
    assert(near(sets[1].whites, -0.13f, 1e-6f));
    assert(near(sets[2].pre_exposure, 0.10f, 1e-6f));
    assert(near(sets[2].saturation, 1.01f, 1e-6f));
    assert(near(sets[3].exposure, -0.06f, 1e-6f));
    assert(near(sets[3].highlights, -0.07f, 1e-6f));
    assert(!uses_pending_controls(sets[3]));
    assert(sets[4].exposure == 0.0f && sets[4].contrast == 1.0f);
}

void the_default_is_lighting_d_with_the_reference_keys() {
    const Settings s = default_settings();
    assert(s.profile_lighting_method == 3.0f);
    assert(near(s.exposure, -0.06f, 1e-6f));
    assert(near(s.contrast, 0.99f, 1e-6f));
    assert(near(s.highlights, -0.07f, 1e-6f));
    assert(near(s.whites, -0.01f, 1e-6f));
    assert(near(s.sharpness, 0.6f, 1e-6f));
    assert(near(s.local_contrast, 0.4f, 1e-6f));
    assert(near(s.ssao_intensity_scale, 1.5f, 1e-6f));
    assert(s.temporal_enabled);
    assert(s.vignette == 0.0f && s.tint == 0.0f);
    assert(s.highlight_rolloff == 0.0f && s.black_lift_g == 0.0f);
}

void the_profile_is_the_only_source_of_the_grade() {
    const Settings s = load_from_text(
        "[profile.photorealism.0.23.0]\n"
        "lighting_method=0\n"
        "sharpness=2\n"
        "sharpen_edges=5\n"
        "ssao_intensity=1.0\n"
        "taa=0\n"
        "[base.0.1.2]\n"
        "enabled=true\n"
        "exposure=0.9\n"
        "tint=0.35\n"
        "[module.user.0.20.0]\n"
        "exposure_delta=0.5\n"
        "[module.condition_adaptation.0.19.0]\n"
        "sun_temperature=3000\n");
    assert(near(s.exposure, -0.09f, 1e-6f));
    assert(near(s.saturation, 0.82f, 1e-6f));
    assert(near(s.vibrance, -0.15f, 1e-6f));
    assert(s.tint == 0.0f && s.temperature == 6500.0f);
    assert(near(s.sharpness, 0.2f, 1e-6f));
    assert(near(s.local_contrast, 0.5f, 1e-6f));
    assert(near(s.ssao_intensity_scale, 1.0f, 1e-6f));
    assert(!s.temporal_enabled);
    assert(near(s.profile_night_exposure, 2.0f, 1e-6f));
    assert(near(night_adjusted_exposure(s, 0.0f), -0.09f, 1e-6f));
    assert(near(night_adjusted_exposure(s, 1.0f), 1.91f, 1e-5f));
    assert(near(night_adjusted_exposure(s, 0.5f), 0.91f, 1e-5f));
}

void editing_the_live_grade_stays_in_its_own_set() {
    Settings s = default_settings();
    s.exposure = 0.40f;
    store_active_tonemap(&s);
    assert(near(s.tonemap_sets[3].exposure, 0.40f, 1e-6f));
    assert(near(s.tonemap_sets[0].exposure, -0.09f, 1e-6f));
    s.profile_lighting_method = 1.0f;
    finish_settings(&s);
    assert(near(s.exposure, 0.25f, 1e-6f));
    s.profile_lighting_method = 3.0f;
    finish_settings(&s);
    assert(near(s.exposure, 0.40f, 1e-6f));
}

void restoring_the_defaults_only_touches_the_profile() {
    Settings s = default_settings();
    s.profile_lighting_method = 0.0f;
    s.tonemap_sets[2].saturation = 1.8f;
    s.profile_fxaa = 0.0f;
    s.fsr_enabled = true;
    restore_profile_defaults(&s);
    finish_settings(&s);
    assert(s.profile_lighting_method == 3.0f);
    assert(near(s.tonemap_sets[2].saturation, 1.01f, 1e-6f));
    assert(s.profile_fxaa == 1.0f);
    assert(s.fsr_enabled);
}

void every_pending_control_is_a_profile_key() {
    for (std::size_t index = 0; index < kPendingControlCount; ++index) {
        assert(profile_key_for(kPendingControls[index].member) != nullptr);
        assert(pending_reason_text(kPendingControls[index].reason) != nullptr);
    }
    assert(profile_key_for(&Settings::exposure) == nullptr);
}

}

int main() {
    keys_land_in_the_numbered_set();
    the_lighting_method_picks_the_set();
    every_set_has_the_reference_values();
    the_default_is_lighting_d_with_the_reference_keys();
    the_profile_is_the_only_source_of_the_grade();
    editing_the_live_grade_stays_in_its_own_set();
    restoring_the_defaults_only_touches_the_profile();
    every_pending_control_is_a_profile_key();
    std::printf("photorealism_profile_test ok\n");
    return 0;
}
