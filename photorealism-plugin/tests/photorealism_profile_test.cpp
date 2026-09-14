#include <windows.h>

#include "../src/config/config.hpp"
#include "../src/config/profile_layer.hpp"

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
    assert(apply_profile_key(&profile, "tonemap_set", "2"));
    assert(apply_profile_key(&profile, "tonemap_exposure_2", "0.25"));
    assert(apply_profile_key(&profile, "tonemap_pre_contrast_2", "0.26"));
    assert(apply_profile_key(&profile, "tonemap_whites_5", "-0.13"));
    assert(apply_profile_key(&profile, "sharpen_edges", "4"));
    assert(near(profile.sets[1].exposure, 0.25f, 1e-6f));
    assert(near(profile.sets[1].pre_contrast, 0.26f, 1e-6f));
    assert(near(profile.sets[4].whites, -0.13f, 1e-6f));
    assert(near(active_tonemap(profile).exposure, 0.25f, 1e-6f));
    assert(uses_pending_controls(active_tonemap(profile)));
}

void malformed_keys_are_refused() {
    PhotorealismProfile profile;
    assert(!apply_profile_key(&profile, "tonemap_set", "7"));
    assert(!apply_profile_key(&profile, "tonemap_exposure_6", "1"));
    assert(!apply_profile_key(&profile, "tonemap_exposure", "1"));
    assert(!apply_profile_key(&profile, "tonemap_tonemap_1", "7"));
    assert(!apply_profile_key(&profile, "lighting_method", "3"));
    assert(profile.tonemap_set == 1);
}

void the_reference_set_becomes_the_base_layer() {
    const CalibrationLayer layer = profile_base_layer(reference_profile());
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
    assert(!uses_pending_controls(active_tonemap(reference_profile())));
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
}

}

int main() {
    keys_land_in_the_numbered_set();
    malformed_keys_are_refused();
    the_reference_set_becomes_the_base_layer();
    the_profile_replaces_the_measured_layers_but_not_the_user();
    without_the_profile_nothing_changes();
    std::printf("photorealism_profile_test ok\n");
    return 0;
}
