#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace photorealism {
wchar_t g_config_path[4096] = {};
const wchar_t* config_path() { return g_config_path; }
void log_message(const char*, ...) {}

namespace config_io {
bool read_file(const wchar_t* path, std::string* contents) {
    FILE* file = _wfopen(path, L"rb");
    if (file == nullptr) {
        return false;
    }
    char buffer[4096];
    std::size_t read = 0;
    contents->clear();
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        contents->append(buffer, read);
    }
    std::fclose(file);
    return true;
}
bool write_atomic(const wchar_t* path, const wchar_t*, const std::string& contents) {
    char narrow[4096] = {};
    ::wcstombs(narrow, path, sizeof(narrow) - 1);
    FILE* file = std::fopen(narrow, "wb");
    if (file == nullptr) {
        return false;
    }
    std::fwrite(contents.data(), 1, contents.size(), file);
    std::fclose(file);
    return true;
}
}
}

#include "../src/config/writer.cpp"
#include "../src/overlay/bindings/binding_values.cpp"
#include "../src/overlay/bindings/menu_pages.cpp"
#include "../src/overlay/persistence.cpp"
#include "config/config.hpp"
#include "config/profile_state.hpp"

#include <cassert>
#include <cmath>

using namespace photorealism;
using namespace photorealism::overlay;

namespace {

const char* kTempPath = "/tmp/photorealism-menu-roundtrip.cfg";

const char* kSeed =
    "[plugin]\n"
    "enabled=true\n"
    "\n"
    "[profile.photorealism.0.23.0]\n"
    "# linha escrita a mao pelo usuario\n"
    "lighting_method=3\n"
    "use_sss=1\n"
    "tonemap_exposure_4=-0.06\n"
    "\n"
    "[module.fsr.0.21.0]\n"
    "enabled=false\n"
    "sharpness=0.60\n";

void write_cfg(const std::string& text) {
    FILE* file = std::fopen(kTempPath, "wb");
    assert(file != nullptr);
    std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
    ::mbstowcs(photorealism::g_config_path, kTempPath, 4095);
}

std::string read_cfg() {
    std::string text;
    assert(config_io::read_file(photorealism::g_config_path, &text));
    return text;
}

bool near(float value, float target) {
    return std::fabs(value - target) < 0.0005f;
}

Settings loaded() {
    Settings settings = {};
    assert(load_settings(&settings));
    return settings;
}

void a_colour_edit_comes_back_in_its_own_set() {
    write_cfg(kSeed);
    const Settings on_disk = loaded();
    Settings live = on_disk;
    live.exposure = 0.37f;
    store_active_tonemap(&live);
    finish_settings(&live);

    assert(save_settings(live, on_disk).written);
    const Settings after = loaded();
    assert(near(after.exposure, 0.37f));
    assert(near(after.tonemap_sets[0].exposure, -0.09f));
    assert(read_cfg().find("tonemap_exposure_4=0.37\n") != std::string::npos);
}

void the_lighting_method_chosen_on_the_menu_comes_back() {
    write_cfg(kSeed);
    const Settings on_disk = loaded();
    Settings live = on_disk;
    live.profile_lighting_method = 1.0f;
    finish_settings(&live);
    assert(near(live.exposure, 0.25f));

    const SaveReport report = save_settings(live, on_disk);
    assert(report.written && report.changed == 1);
    const Settings after = loaded();
    assert(after.profile_lighting_method == 1.0f);
    assert(near(after.exposure, 0.25f));
    assert(near(after.whites, -0.13f));
}

void a_switch_and_a_module_come_back() {
    write_cfg(kSeed);
    const Settings on_disk = loaded();
    Settings live = on_disk;
    live.profile_use_sss = 0.0f;
    live.profile_taa = 0.0f;
    live.fsr_sharpness = 0.35f;
    finish_settings(&live);

    assert(save_settings(live, on_disk).changed == 3);
    const Settings after = loaded();
    assert(after.profile_use_sss == 0.0f);
    assert(!after.temporal_enabled);
    assert(near(after.fsr_sharpness, 0.35f));
    assert(read_cfg().find("# linha escrita a mao pelo usuario\n") != std::string::npos);
}

}

int main() {
    a_colour_edit_comes_back_in_its_own_set();
    the_lighting_method_chosen_on_the_menu_comes_back();
    a_switch_and_a_module_come_back();
    std::remove(kTempPath);
    std::printf("menu_roundtrip_test ok\n");
    return 0;
}
