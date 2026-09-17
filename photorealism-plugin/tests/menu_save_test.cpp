#include <windows.h>

#include "config/settings.hpp"

#include <cassert>
#include <cstdio>
#include <string>

namespace photorealism {

wchar_t g_config_path[8] = {};
const wchar_t* config_path() { return g_config_path; }
void log_message(const char*, ...) {}

std::string g_disk;
std::string g_written;
bool g_write_ok = true;

namespace config_io {
bool read_file(const wchar_t*, std::string* contents) {
    *contents = g_disk;
    return true;
}
bool write_atomic(const wchar_t*, const wchar_t*, const std::string& contents) {
    g_written = contents;
    return g_write_ok;
}
}

}

#include "../src/config/writer.cpp"
#include "../src/overlay/bindings/binding_values.cpp"
#include "../src/overlay/bindings/menu_pages.cpp"
#include "../src/overlay/persistence.cpp"

using namespace photorealism;
using namespace photorealism::overlay;

namespace {

const char* kDisk =
    "# comentario escrito a mao\n"
    "[profile.photorealism.0.23.0]\n"
    "lighting_method=3\n"
    "use_sss=1\n"
    "tonemap_exposure_4=-0.06\n"
    "tonemap_exposure_1=-0.09\n"
    "\n"
    "[module.fsr.0.21.0]\n"
    "enabled=true\n"
    "sharpness=0.35\n";

void seed() {
    g_disk = kDisk;
    g_written.clear();
    g_write_ok = true;
}

Settings disk_state() {
    Settings settings = {};
    settings.profile_lighting_method = 3.0f;
    settings.profile_use_sss = 1.0f;
    settings.fsr_enabled = true;
    settings.fsr_sharpness = 0.35f;
    settings.tonemap_sets[3].exposure = -0.06f;
    settings.tonemap_sets[0].exposure = -0.09f;
    return settings;
}

void a_colour_edit_writes_only_its_set() {
    seed();
    const Settings on_disk = disk_state();
    Settings settings = on_disk;
    settings.tonemap_sets[3].exposure = 0.12f;
    settings.exposure = 0.12f;

    const SaveReport report = save_settings(settings, on_disk);
    assert(report.written && report.changed == 1);
    assert(g_written.find("tonemap_exposure_4=0.12\n") != std::string::npos);
    assert(g_written.find("tonemap_exposure_1=-0.09\n") != std::string::npos);
}

void the_lighting_choice_and_a_switch_reach_the_profile() {
    seed();
    const Settings on_disk = disk_state();
    Settings settings = on_disk;
    settings.profile_lighting_method = 1.0f;
    settings.profile_use_sss = 0.0f;

    const SaveReport report = save_settings(settings, on_disk);
    assert(report.written && report.changed == 2);
    assert(g_written.find("lighting_method=1\n") != std::string::npos);
    assert(g_written.find("use_sss=0\n") != std::string::npos);
}

void the_fsr_page_still_writes_its_module() {
    seed();
    const Settings on_disk = disk_state();
    Settings settings = on_disk;
    settings.fsr_enabled = false;
    settings.fsr_sharpness = 0.6f;

    const SaveReport report = save_settings(settings, on_disk);
    assert(report.changed == 2);
    assert(g_written.find("enabled=false\nsharpness=0.60\n") != std::string::npos);
}

void a_save_without_changes_writes_nothing() {
    seed();
    const Settings on_disk = disk_state();
    const SaveReport report = save_settings(on_disk, on_disk);
    assert(report.written && report.changed == 0);
    assert(g_written.empty());
}

void a_hand_written_comment_survives_a_save() {
    seed();
    const Settings on_disk = disk_state();
    Settings settings = on_disk;
    settings.profile_sharpness = 3.0f;
    save_settings(settings, on_disk);
    assert(g_written.find("# comentario escrito a mao\n") == 0);
    assert(g_written.find("sharpness=3\n") != std::string::npos);
}

}

int main() {
    a_colour_edit_writes_only_its_set();
    the_lighting_choice_and_a_switch_reach_the_profile();
    the_fsr_page_still_writes_its_module();
    a_save_without_changes_writes_nothing();
    a_hand_written_comment_survives_a_save();
    std::printf("menu_save_test ok\n");
    return 0;
}
