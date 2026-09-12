#include <windows.h>

#include "config/calibration.hpp"
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

Settings compose(const CalibrationStack& stack) { return stack.modules; }

}

#include "../src/config/writer.cpp"
#include "../src/overlay/bindings/condition_bindings.cpp"
#include "../src/overlay/bindings/grade_bindings.cpp"
#include "../src/overlay/bindings/pages.cpp"
#include "../src/overlay/bindings/render_bindings.cpp"
#include "../src/overlay/grade_keys.cpp"
#include "../src/overlay/persistence.cpp"

using namespace photorealism;
using namespace photorealism::overlay;

namespace {

const char* kDisk =
    "# a razao medida de cada numero mora nestes comentarios\n"
    "[module.ssao.0.7.0]\n"
    "enabled=true\n"
    "radius=1.40\n"
    "\n"
    "[module.user.0.20.0]\n"
    "enabled=true\n"
    "exposure_delta=0.350000\n"
    "saturation_delta=0.000000\n";

void seed() {
    g_disk = kDisk;
    g_written.clear();
    g_write_ok = true;
}

void the_reset_of_a_colour_slider_reaches_the_cfg() {
    seed();
    Settings baseline = {};
    baseline.exposure = 0.12f;
    Settings on_disk = baseline;
    on_disk.exposure = 0.47f;
    Settings settings = baseline;

    const SaveReport report = save_settings(settings, baseline, on_disk);
    assert(report.written);
    assert(report.changed == 1);
    assert(g_written.find("exposure_delta=0.000000") != std::string::npos);
    assert(g_written.find("exposure_delta=0.350000") == std::string::npos);
}

void the_reset_of_a_module_slider_reaches_the_cfg() {
    seed();
    Settings baseline = {};
    Settings on_disk = baseline;
    on_disk.ssao_radius = 1.4f;
    Settings settings = baseline;
    settings.ssao_radius = 0.8f;

    const SaveReport report = save_settings(settings, baseline, on_disk);
    assert(report.written);
    assert(g_written.find("radius=0.80") != std::string::npos);
    assert(g_written.find("radius=1.40") == std::string::npos);
}

void a_save_without_changes_writes_nothing() {
    seed();
    Settings baseline = {};
    Settings settings = baseline;

    const SaveReport report = save_settings(settings, baseline, settings);
    assert(report.written);
    assert(report.changed == 0);
    assert(g_written.empty());
}

void a_module_turned_off_reaches_the_cfg() {
    seed();
    Settings baseline = {};
    Settings on_disk = baseline;
    on_disk.ssao_enabled = true;
    Settings settings = on_disk;
    settings.ssao_enabled = false;

    const SaveReport report = save_settings(settings, baseline, on_disk);
    assert(report.written);
    assert(g_written.find("enabled=false") != std::string::npos);
}

void the_measured_comments_survive_a_save() {
    seed();
    Settings baseline = {};
    Settings on_disk = baseline;
    on_disk.ssao_radius = 1.4f;
    Settings settings = baseline;
    settings.ssao_radius = 0.8f;

    save_settings(settings, baseline, on_disk);
    assert(
        g_written.find("# a razao medida de cada numero mora nestes comentarios") ==
        0);
}

void the_inert_bloom_controls_are_never_written() {
    seed();
    Settings baseline = {};
    Settings on_disk = baseline;
    Settings settings = baseline;
    settings.bloom_threshold = 0.5f;
    settings.bloom_knee = 0.3f;

    const SaveReport report = save_settings(settings, baseline, on_disk);
    assert(report.changed == 0);
}

}

int main() {
    the_reset_of_a_colour_slider_reaches_the_cfg();
    the_reset_of_a_module_slider_reaches_the_cfg();
    a_save_without_changes_writes_nothing();
    a_module_turned_off_reaches_the_cfg();
    the_measured_comments_survive_a_save();
    the_inert_bloom_controls_are_never_written();
    std::printf("menu_save_test ok\n");
    return 0;
}
