#include "quality_policy.hpp"

#include "../config/path_utils.hpp"
#include "../config/text_utils.hpp"
#include "../native_aa/config_file.hpp"
#include "../native_aa/config_text.hpp"

namespace photorealism {
namespace native_quality {
namespace {

using photorealism::aa_config::plugin_config_value;

QualityLevel level_from_text(const std::string& text) {
    const float value = config_text::clamp_value(
        config_text::to_number(text.c_str()), 0.0f, 2.0f);
    const float rounded = value + 0.5f;
    if (rounded >= 2.0f) {
        return QualityLevel::low;
    }
    if (rounded >= 1.0f) {
        return QualityLevel::medium;
    }
    return QualityLevel::high;
}

void fill_defaults(QualityPolicy* policy) {
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        policy->desired[index] = value_for(kSettings[index], policy->level);
    }
}

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

}  // namespace

QualityPolicy read_native_quality_policy(HMODULE proxy_module) {
    QualityPolicy policy = {};
    policy.manage = true;
    policy.level = QualityLevel::high;
    fill_defaults(&policy);

    wchar_t plugin_config_path[MAX_PATH] = {};
    if (!resolve_plugin_config(proxy_module, plugin_config_path)) {
        return policy;
    }

    std::string plugin_config;
    if (!native_aa::read_file(plugin_config_path, &plugin_config)) {
        return policy;
    }

    const std::string manage =
        plugin_config_value(plugin_config, kNativeQualitySection, "manage");
    policy.manage = manage != "false" && manage != "0";

    const std::string level = plugin_config_value(
        plugin_config, kProfileSectionName, kGlobalQualityKey);
    if (level != kAbsent) {
        policy.level = level_from_text(level);
    }
    fill_defaults(&policy);

    for (std::size_t index = 0; index < kSettingCount; ++index) {
        const std::string override_value = plugin_config_value(
            plugin_config, kNativeQualitySection, kSettings[index].key);
        if (override_value != kAbsent) {
            policy.desired[index] = override_value;
        }
    }
    return policy;
}

}
}
