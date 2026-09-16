#include "native_graphics_config.hpp"

#include "graphics_policy.hpp"
#include "graphics_settings.hpp"
#include "../native_aa/aa_log.hpp"
#include "../native_aa/config_file.hpp"
#include "../native_aa/config_text.hpp"
#include "../native_aa/game_target.hpp"

namespace {

using namespace photorealism::native_graphics;
using photorealism::native_aa::GameTarget;

GraphicsSnapshot read_snapshot(const std::string& contents) {
    GraphicsSnapshot snapshot = {};
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        snapshot.detected[index] =
            photorealism::aa_config::config_value(
                contents, kSettings[index].key);
    }
    return snapshot;
}

bool needs_change(
    const GraphicsSnapshot& snapshot, const GraphicsPolicy& policy,
    std::size_t index) {
    return snapshot.detected[index] != kAbsent &&
           snapshot.detected[index] != policy.desired[index];
}

bool any_change_needed(
    const GraphicsSnapshot& snapshot, const GraphicsPolicy& policy) {
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        if (needs_change(snapshot, policy, index)) {
            return true;
        }
    }
    return false;
}

void log_detected(
    HMODULE module,
    const char* prefix,
    const GameTarget& target,
    const GraphicsSnapshot& snapshot,
    const char* suffix) {
    photorealism::native_aa::log_config(
        module,
        "%s game=%s detected r_ssao=%s; %s",
        prefix,
        target.name,
        snapshot.detected[0].c_str(),
        suffix);
}

void apply_settings(
    std::string* contents,
    const GraphicsSnapshot& snapshot,
    const GraphicsPolicy& policy,
    bool applied[kSettingCount]) {
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        applied[index] =
            needs_change(snapshot, policy, index) &&
            photorealism::aa_config::set_config_value(
                contents,
                kSettings[index].key,
                policy.desired[index].c_str());
    }
}

const char* applied_text(
    const GraphicsPolicy& policy, const bool applied[kSettingCount],
    std::size_t index) {
    return applied[index] ? policy.desired[index].c_str()
                          : "unchanged-or-absent";
}

void log_applied(
    HMODULE module,
    const GameTarget& target,
    const GraphicsSnapshot& snapshot,
    const GraphicsPolicy& policy,
    const bool applied[kSettingCount]) {
    photorealism::native_aa::log_config(
        module,
        "Graphics config: game=%s detected r_ssao=%s; applied r_ssao=%s "
        "backup=config.photorealism-native-aa.backup.cfg (snapshot original, "
        "compartilhado com a configuracao de AA) "
        "timing=bootstrap-before-dxgi politica=photorealism-plugin.cfg; "
        "o SSAO do jogo desligado evita somar com o SSAO do proprio plugin, "
        "que e quem passa a fazer a oclusao.",
        target.name,
        snapshot.detected[0].c_str(),
        applied_text(policy, applied, 0));
}

}

bool configure_native_graphics_for_photorealism(HMODULE proxy_module) {
    GameTarget target = {};
    if (!photorealism::native_aa::identify_running_game(&target)) {
        return false;
    }
    if (!photorealism::native_aa::resolve_documents_config(&target)) {
        photorealism::native_aa::log_config(
            proxy_module,
            "Graphics config: %s Documents/config indisponivel.",
            target.name);
        return true;
    }

    std::string contents;
    if (!photorealism::native_aa::read_file(target.config_path, &contents)) {
        photorealism::native_aa::log_config(
            proxy_module,
            "Graphics config: %s config.cfg ainda indisponivel; sera "
            "tentado na proxima inicializacao.",
            target.name);
        return true;
    }

    const GraphicsSnapshot snapshot = read_snapshot(contents);
    const GraphicsPolicy policy = read_native_graphics_policy(proxy_module);
    if (!policy.manage) {
        log_detected(
            proxy_module,
            "Graphics config:",
            target,
            snapshot,
            "manage=false no photorealism-plugin.cfg; nenhuma alteracao no "
            "config.cfg do jogo.");
        return true;
    }
    if (!any_change_needed(snapshot, policy)) {
        log_detected(
            proxy_module,
            "Graphics config:",
            target,
            snapshot,
            "ja em conformidade com a politica do photorealism-plugin.cfg; "
            "nenhuma escrita necessaria.");
        return true;
    }

    if (!photorealism::native_aa::make_backup(target.config_path)) {
        photorealism::native_aa::log_config(
            proxy_module,
            "Graphics config: game=%s backup falhou; nenhuma configuracao "
            "grafica foi alterada.",
            target.name);
        return true;
    }

    bool applied[kSettingCount] = {};
    apply_settings(&contents, snapshot, policy, applied);
    if (!photorealism::native_aa::write_atomic(target.config_path, contents)) {
        photorealism::native_aa::log_config(
            proxy_module,
            "Graphics config: game=%s escrita atomica falhou; backup "
            "preservado.",
            target.name);
        return true;
    }
    log_applied(proxy_module, target, snapshot, policy, applied);
    return true;
}
