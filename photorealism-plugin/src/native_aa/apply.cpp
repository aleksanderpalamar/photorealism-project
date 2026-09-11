#include "native_aa_config.hpp"

#include "aa_log.hpp"
#include "aa_settings.hpp"
#include "config_file.hpp"
#include "config_text.hpp"
#include "game_target.hpp"
#include "policy.hpp"

namespace {

using namespace photorealism::native_aa;

AaSnapshot read_snapshot(const std::string& contents) {
    AaSnapshot snapshot = {};
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        snapshot.detected[index] =
            photorealism::aa_config::config_value(
                contents, kSettings[index].key);
    }
    return snapshot;
}

bool needs_change(
    const AaSnapshot& snapshot, const AaPolicy& policy, std::size_t index) {
    return snapshot.detected[index] != kAbsent &&
           snapshot.detected[index] != policy.desired[index];
}

bool any_change_needed(const AaSnapshot& snapshot, const AaPolicy& policy) {
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
    const AaSnapshot& snapshot,
    const char* suffix) {
    log_config(
        module,
        "%s game=%s detected r_aa=%s r_taa_tuning=%s "
        "r_taa_luma_sharpen=%s r_taa_modulated_drr_strength=%s; %s",
        prefix,
        target.name,
        snapshot.detected[0].c_str(),
        snapshot.detected[1].c_str(),
        snapshot.detected[2].c_str(),
        snapshot.detected[3].c_str(),
        suffix);
}

void apply_settings(
    std::string* contents,
    const AaSnapshot& snapshot,
    const AaPolicy& policy,
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
    const AaPolicy& policy, const bool applied[kSettingCount], std::size_t index) {
    return applied[index] ? policy.desired[index].c_str()
                          : "unchanged-or-absent";
}

void log_applied(
    HMODULE module,
    const GameTarget& target,
    const AaSnapshot& snapshot,
    const AaPolicy& policy,
    const bool applied[kSettingCount]) {
    log_config(
        module,
        "AA config: game=%s detected r_aa=%s r_taa_tuning=%s "
        "r_taa_luma_sharpen=%s r_taa_modulated_drr_strength=%s; applied "
        "r_aa=%s r_taa_tuning=%s r_taa_luma_sharpen=%s "
        "r_taa_modulated_drr_strength=%s backup="
        "config.photorealism-native-aa.backup.cfg "
        "timing=bootstrap-before-dxgi politica=photorealism-plugin.cfg; "
        "o TAA nativo ligado e o que faz o Prism3D expor o depth como shader "
        "resource, de que SSAO e o resolve temporal dependem.",
        target.name,
        snapshot.detected[0].c_str(),
        snapshot.detected[1].c_str(),
        snapshot.detected[2].c_str(),
        snapshot.detected[3].c_str(),
        applied_text(policy, applied, 0),
        applied_text(policy, applied, 1),
        applied_text(policy, applied, 2),
        applied_text(policy, applied, 3));
}

}

bool configure_native_aa_for_photorealism(HMODULE proxy_module) {
    GameTarget target = {};
    if (!identify_running_game(&target)) {
        return false;
    }
    if (!resolve_documents_config(&target)) {
        log_config(
            proxy_module,
            "AA config: %s Documents/config indisponivel.",
            target.name);
        return true;
    }

    std::string contents;
    if (!read_file(target.config_path, &contents)) {
        log_config(
            proxy_module,
            "AA config: %s config.cfg ainda indisponivel; sera tentado na "
            "proxima inicializacao.",
            target.name);
        return true;
    }

    const AaSnapshot snapshot = read_snapshot(contents);
    const AaPolicy policy = read_native_aa_policy(proxy_module);
    if (!policy.manage) {
        log_detected(
            proxy_module,
            "AA config:",
            target,
            snapshot,
            "manage=false no photorealism-plugin.cfg; nenhuma alteracao no "
            "config.cfg do jogo.");
        return true;
    }
    if (!any_change_needed(snapshot, policy)) {
        log_detected(
            proxy_module,
            "AA config:",
            target,
            snapshot,
            "ja em conformidade com a politica do photorealism-plugin.cfg; "
            "nenhuma escrita necessaria.");
        return true;
    }

    if (!make_backup(target.config_path)) {
        log_config(
            proxy_module,
            "AA config: game=%s backup falhou; nenhuma configuracao AA/TAA "
            "foi alterada.",
            target.name);
        return true;
    }

    bool applied[kSettingCount] = {};
    apply_settings(&contents, snapshot, policy, applied);
    if (!write_atomic(target.config_path, contents)) {
        log_config(
            proxy_module,
            "AA config: game=%s escrita atomica falhou; backup preservado.",
            target.name);
        return true;
    }
    log_applied(proxy_module, target, snapshot, policy, applied);
    return true;
}
