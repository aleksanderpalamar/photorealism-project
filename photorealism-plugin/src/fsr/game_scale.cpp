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
constexpr const wchar_t* kSavedScale = L"config.photorealism-fsr-scale.saved";

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

bool fsr_is_enabled(const std::string& plugin_config) {
    const std::string enabled = aa_config::plugin_config_value(
        plugin_config, kFsrSection, "enabled");
    return enabled == "true" || enabled == "1";
}

bool sibling_of(const wchar_t* config_path, const wchar_t* name, wchar_t* out) {
    std::wcsncpy(out, config_path, MAX_PATH - 1);
    if (!paths::keep_directory_of(out)) {
        return false;
    }
    return paths::append_path(out, MAX_PATH, name);
}

std::string read_saved_scale(const wchar_t* config_path) {
    wchar_t saved_path[MAX_PATH] = {};
    if (!sibling_of(config_path, kSavedScale, saved_path)) {
        return std::string();
    }
    std::string saved;
    if (!config_io::read_file(saved_path, &saved)) {
        return std::string();
    }
    while (!saved.empty() &&
           (saved.back() == '\n' || saved.back() == '\r' ||
            saved.back() == ' ')) {
        saved.pop_back();
    }
    return saved;
}

void remember_scale(const wchar_t* config_path, const std::string& value) {
    wchar_t saved_path[MAX_PATH] = {};
    if (!sibling_of(config_path, kSavedScale, saved_path)) {
        return;
    }
    std::string existing;
    if (config_io::read_file(saved_path, &existing)) {
        return;
    }
    config_io::write_atomic(saved_path, kTemporary, value);
}

void forget_scale(const wchar_t* config_path) {
    wchar_t saved_path[MAX_PATH] = {};
    if (sibling_of(config_path, kSavedScale, saved_path)) {
        DeleteFileW(saved_path);
    }
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

bool rewrite_scale(
    std::string* contents, const std::string& printed, std::string* before) {
    bool changed = false;
    for (const char* key : kScaleKeys) {
        const std::string current = aa_config::config_value(*contents, key);
        if (before->empty()) {
            *before = current;
        }
        if (current == "ausente" || current == printed) {
            continue;
        }
        changed =
            aa_config::set_config_value(contents, key, printed.c_str()) ||
            changed;
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

    native_aa::GameTarget target = {};
    if (!native_aa::identify_running_game(&target) ||
        !native_aa::resolve_documents_config(&target)) {
        return;
    }

    std::string game_config;
    if (!config_io::read_file(target.config_path, &game_config)) {
        native_aa::log_config(
            proxy_module, "FSR escala: config do jogo ilegivel.");
        return;
    }

    const bool enabled = fsr_is_enabled(plugin_config);
    const std::string saved = read_saved_scale(target.config_path);
    if (!enabled && saved.empty()) {
        native_aa::log_config(
            proxy_module,
            "FSR escala: desligado e nada emprestado -- o Scaling das opcoes "
            "graficas do %s continua sendo seu.",
            target.name);
        return;
    }

    std::string before;
    std::string wanted = saved;
    if (enabled) {
        char printed[32] = {};
        std::snprintf(
            printed,
            sizeof(printed),
            "%.6f",
            static_cast<double>(desired_scale(plugin_config)));
        wanted = printed;
    }

    if (!rewrite_scale(&game_config, wanted, &before)) {
        native_aa::log_config(
            proxy_module,
            "FSR escala: r_scale ja em %s, nenhuma alteracao no %s.",
            wanted.c_str(),
            target.name);
        if (!enabled) {
            forget_scale(target.config_path);
        }
        return;
    }

    if (enabled) {
        remember_scale(target.config_path, before);
    }
    const bool written = config_io::write_atomic(
        target.config_path, kTemporary, game_config);
    if (written && !enabled) {
        forget_scale(target.config_path);
    }
    native_aa::log_config(
        proxy_module,
        "FSR escala: r_scale_x/y de %s para %s no %s (%s). Gravacao %s.",
        before.c_str(),
        wanted.c_str(),
        target.name,
        enabled ? "emprestado pelo FSR, guardado para devolver"
                : "devolvido ao valor que era seu",
        written ? "ok" : "falhou");
}

}
}
