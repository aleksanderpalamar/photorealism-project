#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace photorealism {
wchar_t g_config_path[4096] = {};
const wchar_t* config_path() { return g_config_path; }
void log_message(const char*, ...) {}
}

#include "../src/config/writer.cpp"
#include "../src/overlay/grade_keys.cpp"
#include "config/config.hpp"

#include <cassert>
#include <cmath>
#include <string>

using namespace photorealism;
using namespace photorealism::overlay;

namespace {

const char* kTempPath = "/tmp/photorealism-menu-roundtrip.cfg";

const char* kSeed =
    "[profile.photorealism.0.23.0]\n"
    "enabled=false\n"
    "\n"
    "[base.0.1.2]\n"
    "enabled=true\n"
    "exposure=0.20\n"
    "saturation=1.00\n"
    "\n"
    "[module.visual.0.2.0]\n"
    "enabled=true\n"
    "exposure_delta=0.05\n"
    "\n"
    "[module.rain_overcast.0.3.0]\n"
    "enabled=true\n"
    "# esta linha guarda a razao medida do numero abaixo\n"
    "exposure_delta=0.02\n"
    "\n"
    "[module.ssao.0.7.0]\n"
    "enabled=true\n"
    "radius=0.8\n";

void write_cfg(const std::string& text) {
    FILE* file = std::fopen(kTempPath, "wb");
    assert(file != nullptr);
    std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
    ::mbstowcs(photorealism::g_config_path, kTempPath, 4095);
}

std::string read_cfg() {
    FILE* file = std::fopen(kTempPath, "rb");
    assert(file != nullptr);
    std::string text;
    char buffer[4096];
    std::size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        text.append(buffer, read);
    }
    std::fclose(file);
    return text;
}

bool near(float value, float target) {
    return std::fabs(value - target) < 0.0005f;
}

void the_measured_layers_still_sum_without_a_user_layer() {
    write_cfg(kSeed);
    Settings settings = {};
    assert(load_settings(&settings));
    assert(near(settings.exposure, 0.27f));
}

void what_the_menu_saves_is_what_the_loader_gives_back() {
    write_cfg(kSeed);
    Settings before = {};
    assert(load_settings(&before));

    const float on_screen = 0.55f;
    const float delta = on_screen - before.exposure;
    char printed[32] = {};
    std::snprintf(printed, sizeof(printed), "%.6f", static_cast<double>(delta));

    std::string text = read_cfg();
    assert(config_writer::set_value(
        &text, kUserSection, grade_key_for(&Settings::exposure), printed));
    write_cfg(text);

    Settings after = {};
    assert(load_settings(&after));
    assert(near(after.exposure, on_screen));
}

void saving_never_touches_the_measured_layers() {
    write_cfg(kSeed);
    Settings before = {};
    assert(load_settings(&before));

    const float wanted = before.saturation + 0.25f;
    char printed[32] = {};
    std::snprintf(
        printed,
        sizeof(printed),
        "%.6f",
        static_cast<double>(wanted - before.saturation));

    std::string text = read_cfg();
    assert(config_writer::set_value(
        &text, kUserSection, "saturation_delta", printed));
    assert(config_writer::set_value(
        &text, "module.ssao.0.7.0", "radius", "1.40"));
    write_cfg(text);

    const std::string result = read_cfg();
    assert(result.find("[base.0.1.2]\nenabled=true\nexposure=0.20\n") !=
           std::string::npos);
    assert(result.find("exposure_delta=0.05") != std::string::npos);
    assert(result.find("exposure_delta=0.02") != std::string::npos);
    assert(result.find("# esta linha guarda a razao medida do numero abaixo") !=
           std::string::npos);

    Settings after = {};
    assert(load_settings(&after));
    assert(near(after.saturation, wanted));
    assert(near(after.ssao_radius, 1.4f));
    assert(near(after.exposure, before.exposure));
}

void with_the_profile_on_the_menu_value_also_comes_back() {
    std::string seed = kSeed;
    seed.replace(seed.find("enabled=false"), 13, "enabled=true");
    write_cfg(seed);
    Settings before = {};
    assert(load_settings(&before));
    assert(before.photorealism_profile_enabled);

    const float on_screen = 0.31f;
    char printed[32] = {};
    std::snprintf(
        printed, sizeof(printed), "%.6f",
        static_cast<double>(on_screen - before.exposure));
    std::string text = read_cfg();
    assert(config_writer::set_value(
        &text, kUserSection, grade_key_for(&Settings::exposure), printed));
    write_cfg(text);

    Settings after = {};
    assert(load_settings(&after));
    assert(near(after.exposure, on_screen));
}

void a_zeroed_user_layer_changes_nothing() {
    write_cfg(kSeed);
    Settings before = {};
    assert(load_settings(&before));

    std::string text = read_cfg();
    for (std::size_t index = 0; index < kGradeKeyCount; ++index) {
        assert(config_writer::set_value(
            &text, kUserSection, kGradeKeys[index].key, "0.000000"));
    }
    write_cfg(text);

    Settings after = {};
    assert(load_settings(&after));
    assert(near(after.exposure, before.exposure));
    assert(near(after.saturation, before.saturation));
}

}

int main() {
    the_measured_layers_still_sum_without_a_user_layer();
    what_the_menu_saves_is_what_the_loader_gives_back();
    saving_never_touches_the_measured_layers();
    with_the_profile_on_the_menu_value_also_comes_back();
    a_zeroed_user_layer_changes_nothing();
    std::remove(kTempPath);
    std::printf("menu_roundtrip_test ok\n");
    return 0;
}
