#include "native_quality_config.hpp"

#include "../native_aa/aa_log.hpp"
#include "../native_aa/config_file.hpp"
#include "../native_aa/config_text.hpp"
#include "../native_aa/game_target.hpp"
#include "quality_plan.hpp"
#include "quality_policy.hpp"

#include <string>

namespace {

using namespace photorealism::native_quality;
using photorealism::native_aa::GameTarget;

}  // namespace

bool configure_native_quality_for_photorealism(HMODULE proxy_module) {
    GameTarget target = {};
    if (!photorealism::native_aa::identify_running_game(&target)) {
        return false;
    }
    if (!photorealism::native_aa::resolve_documents_config(&target)) {
        photorealism::native_aa::log_config(
            proxy_module,
            "Qualidade: %s Documents/config indisponivel.",
            target.name);
        return true;
    }

    std::string contents;
    if (!photorealism::native_aa::read_file(target.config_path, &contents)) {
        photorealism::native_aa::log_config(
            proxy_module,
            "Qualidade: %s config.cfg ainda indisponivel; sera tentado na "
            "proxima inicializacao.",
            target.name);
        return true;
    }

    const QualityPolicy policy = read_native_quality_policy(proxy_module);
    const QualitySnapshot snapshot = read_snapshot(contents);

    if (!policy.manage) {
        photorealism::native_aa::log_config(
            proxy_module,
            "Qualidade: game=%s manage=false no photorealism-plugin.cfg; as "
            "opcoes graficas do jogo continuam suas. Detectado: %s",
            target.name,
            describe(snapshot, policy, false).c_str());
        return true;
    }

    const unsigned pending = count_changes(snapshot, policy);
    if (pending == 0) {
        photorealism::native_aa::log_config(
            proxy_module,
            "Qualidade %s: game=%s as %zu opcoes graficas ja estao conforme o "
            "menu; nenhuma escrita necessaria.",
            level_name(policy.level),
            target.name,
            kSettingCount);
        return true;
    }

    if (!photorealism::native_aa::make_backup(target.config_path)) {
        photorealism::native_aa::log_config(
            proxy_module,
            "Qualidade: game=%s backup falhou; nenhuma opcao grafica foi "
            "alterada.",
            target.name);
        return true;
    }

    const std::string changes = describe(snapshot, policy, true);
    const unsigned applied = apply_settings(&contents, snapshot, policy);
    if (!photorealism::native_aa::write_atomic(target.config_path, contents)) {
        photorealism::native_aa::log_config(
            proxy_module,
            "Qualidade: game=%s escrita atomica falhou; backup preservado.",
            target.name);
        return true;
    }

    photorealism::native_aa::log_config(
        proxy_module,
        "Qualidade %s: game=%s %u de %zu opcoes graficas do jogo escritas "
        "(%u pendentes). backup=config.photorealism-native-aa.backup.cfg "
        "timing=bootstrap-before-dxgi politica=photorealism-plugin.cfg. "
        "Mudancas: %s",
        level_name(policy.level),
        target.name,
        applied,
        kSettingCount,
        pending,
        changes.c_str());
    return true;
}
