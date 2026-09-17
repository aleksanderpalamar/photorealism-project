#include <windows.h>

#include "../src/config/config.hpp"
#include "../src/config/profile_fields.hpp"

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

const char* kTempPath = "/tmp/photorealism-config-load-test.cfg";

Settings load_from_text(const char* text) {
    FILE* file = std::fopen(kTempPath, "wb");
    assert(file != nullptr);
    std::fwrite(text, 1, std::strlen(text), file);
    std::fclose(file);
    ::mbstowcs(photorealism::g_config_path, kTempPath, 4095);
    Settings settings = {};
    const bool found = load_settings(&settings);
    assert(found);
    std::remove(kTempPath);
    return settings;
}

Settings load_missing_file() {
    ::mbstowcs(photorealism::g_config_path, "/tmp/nao-existe-photorealism.cfg", 4095);
    Settings settings = {};
    const bool found = load_settings(&settings);
    assert(!found);
    return settings;
}

bool near(float value, float target, float tolerance) {
    return std::fabs(value - target) <= tolerance;
}

std::string shipped_config_path() {
    const char* root = std::getenv("PHOTOREALISM_PROJECT_DIR");
    const std::string base = root != nullptr ? root : ".";
    return base + "/config/photorealism-plugin.cfg";
}

void the_shipped_cfg_and_the_internal_defaults_agree() {
    const Settings internal = load_missing_file();
    const std::string path = shipped_config_path();
    ::mbstowcs(photorealism::g_config_path, path.c_str(), 4095);
    Settings shipped = {};
    assert(load_settings(&shipped));
    assert(near(internal.temperature, shipped.temperature, 0.01f));
    assert(near(internal.exposure, shipped.exposure, 1e-5f));
    assert(near(internal.contrast, shipped.contrast, 1e-5f));
    assert(near(internal.saturation, shipped.saturation, 1e-5f));
    assert(near(internal.vibrance, shipped.vibrance, 1e-5f));
    assert(near(internal.shadows, shipped.shadows, 1e-5f));
    assert(near(internal.highlights, shipped.highlights, 1e-5f));
    assert(near(internal.blacks, shipped.blacks, 1e-5f));
    assert(near(internal.whites, shipped.whites, 1e-5f));
    assert(near(internal.local_contrast, shipped.local_contrast, 1e-5f));
    assert(near(internal.sharpness, shipped.sharpness, 1e-5f));
    assert(near(internal.ssao_intensity_scale, shipped.ssao_intensity_scale, 1e-6f));
    assert(internal.temporal_enabled == shipped.temporal_enabled);
    for (std::size_t index = 0; index < kProfileFieldCount; ++index) {
        const ModuleField& field = kProfileFields[index];
        assert(near(internal.*(field.member), shipped.*(field.member), 1e-6f));
    }
    assert(internal.fsr_enabled == shipped.fsr_enabled);
    assert(near(internal.fsr_render_scale, shipped.fsr_render_scale, 1e-6f));
    assert(near(internal.bloom_intensity, shipped.bloom_intensity, 1e-6f));
}

void unknown_sections_and_keys_are_ignored() {
    const Settings s = load_from_text(
        "[module.inventado.9.9.9]\n"
        "sharpness=99\n"
        "[module.ssao.0.7.0]\n"
        "radius=1.25\n"
        "[base.0.1.2]\n"
        "exposure=1.5\n"
        "[module.bloom.0.17.0]\n"
        "chave_que_nao_existe=123\n"
        "threshold=0.9\n");
    const Settings internal = load_missing_file();
    assert(near(s.bloom_threshold, 0.9f, 1e-5f));
    assert(near(s.ssao_radius, internal.ssao_radius, 1e-6f));
    assert(near(s.exposure, internal.exposure, 1e-6f));
    assert(near(s.sharpness, internal.sharpness, 1e-6f));
}

void values_out_of_range_are_clamped() {
    const Settings s = load_from_text(
        "[module.bloom.0.17.0]\n"
        "threshold=5.0\n"
        "[profile.photorealism.0.23.0]\n"
        "lighting_method=9\n"
        "taa=4\n"
        "sharpness=40\n"
        "tonemap_exposure_4=7\n");
    assert(near(s.bloom_threshold, 0.98f, 1e-5f));
    assert(s.profile_lighting_method == 3.0f);
    assert(s.profile_taa == 2.0f);
    assert(s.temporal_enabled);
    assert(near(s.sharpness, 1.0f, 1e-6f));
    assert(near(s.exposure, 2.0f, 1e-6f));
}

void whitespace_and_comments_are_tolerated() {
    const Settings s = load_from_text(
        "# comentario\n"
        "; outro comentario\n"
        "\n"
        "   [ profile.photorealism.0.23.0 ]   \n"
        "   sharpen_edges   =   3   \n"
        "linha sem igual\n"
        "ssao_intensity=0.5\n");
    assert(near(s.local_contrast, 0.3f, 1e-6f));
    assert(near(s.ssao_intensity_scale, 0.5f, 1e-6f));
}

void module_switches_are_read() {
    const Settings s = load_from_text(
        "[plugin]\n"
        "enabled=false\n"
        "[module.bloom.0.17.0]\n"
        "enabled=false\n"
        "[module.fsr.0.21.0]\n"
        "enabled=true\n");
    assert(!s.enabled);
    assert(!s.bloom_enabled);
    assert(s.fsr_enabled);
    assert(load_from_text("[module.bloom.0.17.0]\nenabled=yes\n").bloom_enabled);
    assert(load_from_text("[module.bloom.0.17.0]\nenabled=TRUE\n").bloom_enabled);
    assert(load_from_text("[module.bloom.0.17.0]\nenabled=1\n").bloom_enabled);
    assert(!load_from_text("[module.bloom.0.17.0]\nenabled=0\n").bloom_enabled);
    assert(!load_from_text("[module.bloom.0.17.0]\nenabled=nao\n").bloom_enabled);
}

void an_empty_file_equals_the_internal_defaults() {
    const Settings empty = load_from_text("");
    const Settings internal = load_missing_file();
    assert(near(empty.temperature, internal.temperature, 0.01f));
    assert(near(empty.exposure, internal.exposure, 1e-6f));
    assert(empty.profile_lighting_method == internal.profile_lighting_method);
}

}

int main() {
    the_shipped_cfg_and_the_internal_defaults_agree();
    unknown_sections_and_keys_are_ignored();
    values_out_of_range_are_clamped();
    whitespace_and_comments_are_tolerated();
    module_switches_are_read();
    an_empty_file_equals_the_internal_defaults();
    std::printf("config_load_test ok\n");
    return 0;
}
