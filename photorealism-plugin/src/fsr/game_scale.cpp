#include "game_scale.hpp"

#include "../config/file_io.hpp"
#include "../config/path_utils.hpp"
#include "../native_aa/aa_log.hpp"
#include "../native_aa/config_text.hpp"
#include "../native_aa/game_target.hpp"
#include "render_scale.hpp"

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>

namespace photorealism {
namespace fsr {
namespace {

constexpr const char* kFsrSection = "module.fsr.0.21.0";
constexpr const char* kScaleKeys[] = {"r_scale_x", "r_scale_y"};
constexpr const wchar_t* kTemporary = L"config.photorealism-fsr.tmp";

bool resolve_plugin_config(HMODULE proxy_module, wchar_t* path) {
    if (GetModuleFileNameW(proxy_module, path, MAX_PATH) == 0) {
        return false;
    }
    if (!paths::keep_directory_of(path)) {
        return false;
    }
    return paths::append_path(
        path, MAX_PATH, L"photorealism-plugin\\photorealism-plugin.cfg");
}

float desired_scale(const std::string& plugin_config) {
    const std::string enabled = aa_config::plugin_config_value(
        plugin_config, kFsrSection, "enabled");
    if (enabled != "true" && enabled != "1") {
        return 1.0f;
    }
    const std::string scale = aa_config::plugin_config_value(
        plugin_config, kFsrSection, "render_scale");
    if (scale == "ausente") {
        return 1.0f;
    }
    return clamp_scale(static_cast<float>(std::atof(scale.c_str())));
}

bool rewrite_scale(std::string* contents, float scale, std::string* before) {
    char printed[32] = {};
    std::snprintf(printed, sizeof(printed), "%.6f", static_cast<double>(scale));

    bool changed = false;
    for (const char* key : kScaleKeys) {
        const std::string current = aa_config::config_value(*contents, key);
        if (before->empty()) {
            *before = current;
        }
        if (current == "ausente" || current == printed) {
            continue;
        }
        changed = aa_config::set_config_value(contents, key, printed) || changed;
    }
    return changed;
}

}

void apply_render_scale_to_game(HMODULE proxy_module) {
    wchar_t plugin_config_path[MAX_PATH] = {};
    if (!resolve_plugin_config(proxy_module, plugin_config_path)) {
        return;
    }
    std::string plugin_config;
    if (!config_io::read_file(plugin_config_path, &plugin_config)) {
        return;
    }
    const float scale = desired_scale(plugin_config);

    native_aa::GameTarget target = {};
    if (!native_aa::identify_running_game(&target) ||
        !native_aa::resolve_documents_config(&target)) {
        native_aa::log_config(
            proxy_module, "FSR escala: config do jogo nao encontrado.");
        return;
    }

    std::string game_config;
    if (!config_io::read_file(target.config_path, &game_config)) {
        native_aa::log_config(
            proxy_module, "FSR escala: config do jogo ilegivel.");
        return;
    }

    std::string before;
    if (!rewrite_scale(&game_config, scale, &before)) {
        native_aa::log_config(
            proxy_module,
            "FSR escala: r_scale ja em %.4f, nenhuma alteracao no %s.",
            static_cast<double>(scale),
            target.name);
        return;
    }

    const bool written = config_io::write_atomic(
        target.config_path, kTemporary, game_config);
    native_aa::log_config(
        proxy_module,
        "FSR escala: r_scale_x/y de %s para %.4f no %s -- o Prism3D passa a "
        "desenhar a cena nessa fracao de cada lado. Gravacao %s.",
        before.c_str(),
        static_cast<double>(scale),
        target.name,
        written ? "ok" : "falhou");
}

}
}
