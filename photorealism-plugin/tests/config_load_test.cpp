#include <windows.h>

#include "../src/config.hpp"

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
void set_module(HMODULE) {}
const wchar_t* module_directory() { return L"."; }
const wchar_t* plugin_root() { return L"."; }
const wchar_t* shader_path() { return L"."; }
const wchar_t* depth_preview_shader_path() { return L"."; }
const wchar_t* ssao_shader_path() { return L"."; }
const wchar_t* temporal_shader_path() { return L"."; }
const wchar_t* bloom_shader_path() { return L"."; }
}

#include "../src/config.cpp"

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
}

int main() {
    {
        const Settings internal = load_missing_file();
        const std::string path = shipped_config_path();
        ::mbstowcs(photorealism::g_config_path, path.c_str(), 4095);
        Settings shipped = {};
        if (load_settings(&shipped)) {
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
            assert(near(internal.vignette, shipped.vignette, 1e-5f));
            assert(near(internal.tint, shipped.tint, 1e-5f));
            assert(near(internal.highlight_rolloff, shipped.highlight_rolloff, 1e-5f));
            assert(near(internal.black_lift_r, shipped.black_lift_r, 1e-7f));
            assert(near(internal.black_lift_g, shipped.black_lift_g, 1e-7f));
            assert(near(internal.black_lift_b, shipped.black_lift_b, 1e-7f));

            assert(near(internal.condition_sun_temperature,
                        shipped.condition_sun_temperature, 0.01f));
            assert(near(internal.condition_rain_temperature,
                        shipped.condition_rain_temperature, 0.01f));
            assert(near(internal.condition_night_temperature,
                        shipped.condition_night_temperature, 0.01f));
            assert(near(internal.condition_sun_tint, shipped.condition_sun_tint, 1e-5f));
            assert(near(internal.condition_rain_tint, shipped.condition_rain_tint, 1e-5f));
            assert(near(internal.condition_night_tint, shipped.condition_night_tint, 1e-5f));
        }
    }

    {
        const Settings s = load_from_text(
            "[base.0.1.2]\n"
            "enabled=true\n"
            "temperature=6000\n"
            "tint=0.20\n"
            "[module.visual.0.2.0]\n"
            "enabled=true\n"
            "temperature_delta=200\n"
            "tint_delta=0.10\n"
            "[module.rain_overcast.0.3.0]\n"
            "enabled=true\n"
            "temperature_delta=100\n"
            "tint_delta=0.05\n");
        assert(near(s.temperature, 6300.0f, 0.01f));
        assert(near(s.tint, 0.35f, 1e-5f));
    }

    {
        const Settings s = load_from_text(
            "[base.0.1.2]\n"
            "enabled=true\n"
            "temperature=6000\n"
            "[module.visual.0.2.0]\n"
            "enabled=false\n"
            "temperature_delta=500\n"
            "[module.rain_overcast.0.3.0]\n"
            "enabled=false\n"
            "temperature_delta=500\n");
        assert(near(s.temperature, 6000.0f, 0.01f));
    }

    {
        const Settings s = load_from_text(
            "[base.0.1.2]\n"
            "enabled=true\n"
            "black_lift=0.0027\n"
            "[module.visual.0.2.0]\n"
            "enabled=false\n"
            "[module.rain_overcast.0.3.0]\n"
            "enabled=false\n");
        assert(near(s.black_lift_r, 0.0027f, 1e-7f));
        assert(near(s.black_lift_g, 0.0027f, 1e-7f));
        assert(near(s.black_lift_b, 0.0027f, 1e-7f));
    }

    {
        const Settings s = load_from_text(
            "[module.inventado.9.9.9]\n"
            "sun_temperature=3000\n"
            "radius=99\n"
            "[module.ssao.0.7.0]\n"
            "radius=1.25\n");
        assert(near(s.ssao_radius, 1.25f, 1e-5f));
        assert(near(s.condition_sun_temperature, 5900.0f, 0.01f));
    }

    {
        const Settings s = load_from_text(
            "[module.bloom.0.17.0]\n"
            "chave_que_nao_existe=123\n"
            "threshold=0.9\n");
        assert(near(s.bloom_threshold, 0.9f, 1e-5f));
    }

    {
        const Settings s = load_from_text(
            "[module.bloom.0.17.0]\n"
            "threshold=5.0\n"
            "[module.condition_adaptation.0.19.0]\n"
            "rain_temperature=99999\n"
            "sun_tint=-40\n");

        assert(near(s.bloom_threshold, 0.98f, 1e-5f));
        assert(near(s.condition_rain_temperature, 9000.0f, 0.01f));
        assert(near(s.condition_sun_tint, -1.0f, 1e-5f));
    }

    {
        const Settings s = load_from_text(
            "[module.condition_adaptation.0.19.0]\n"
            "daylight_median_low=50\n"
            "daylight_median_high=10\n"
            "overcast_saturation_low=0.30\n"
            "overcast_saturation_high=0.05\n");
        assert(s.condition_daylight_median_high > s.condition_daylight_median_low);
        assert(s.condition_overcast_saturation_high >
               s.condition_overcast_saturation_low);
    }

    {
        const Settings s = load_from_text(
            "# comentario\n"
            "; outro comentario\n"
            "\n"
            "   [ module.ssao.0.7.0 ]   \n"
            "   radius   =   0.55   \n"
            "linha sem igual\n"
            "intensity=0.33\n");
        assert(near(s.ssao_radius, 0.55f, 1e-5f));
        assert(near(s.ssao_intensity, 0.33f, 1e-5f));
    }

    {
        const Settings s = load_from_text(
            "[plugin]\n"
            "enabled=true\n"
            "[module.ssao.0.7.0]\n"
            "enabled=false\n"
            "[module.bloom.0.17.0]\n"
            "enabled=false\n");
        assert(s.enabled);
        assert(!s.ssao_enabled);
        assert(!s.bloom_enabled);
        assert(s.temporal_enabled);
        assert(s.condition_adaptation_enabled);
    }
    {
        const Settings s = load_from_text(
            "[plugin]\n"
            "enabled=false\n"
            "[module.ssao.0.7.0]\n"
            "enabled=true\n");
        assert(!s.enabled);
        assert(s.ssao_enabled);
    }

    {
        assert(load_from_text("[module.bloom.0.17.0]\nenabled=yes\n").bloom_enabled);
        assert(load_from_text("[module.bloom.0.17.0]\nenabled=TRUE\n").bloom_enabled);
        assert(load_from_text("[module.bloom.0.17.0]\nenabled=1\n").bloom_enabled);
        assert(!load_from_text("[module.bloom.0.17.0]\nenabled=0\n").bloom_enabled);
        assert(!load_from_text("[module.bloom.0.17.0]\nenabled=nao\n").bloom_enabled);
    }

    {
        const Settings empty = load_from_text("");
        const Settings internal = load_missing_file();
        assert(near(empty.temperature, internal.temperature, 0.01f));
        assert(near(empty.exposure, internal.exposure, 1e-6f));
        assert(near(empty.tint, internal.tint, 1e-6f));
        assert(near(empty.condition_rain_temperature,
                    internal.condition_rain_temperature, 0.01f));
    }

    return 0;
}
