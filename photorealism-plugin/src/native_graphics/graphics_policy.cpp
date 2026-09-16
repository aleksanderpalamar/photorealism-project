#include "graphics_policy.hpp"

#include "../native_aa/config_file.hpp"
#include "../native_aa/config_text.hpp"
#include "../config/path_utils.hpp"

#include <cwchar>

namespace photorealism {
namespace native_graphics {
namespace {

GraphicsPolicy default_policy() {
    GraphicsPolicy policy = {};
    policy.manage = true;
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        policy.desired[index] = kSettings[index].fallback;
    }
    return policy;
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

std::string policy_value(
    const std::string& plugin_config, const char* key, const char* fallback) {
    const std::string value = photorealism::aa_config::plugin_config_value(
        plugin_config, kNativeGraphicsSection, key);
    return value == kAbsent ? std::string(fallback) : value;
}

}

GraphicsPolicy read_native_graphics_policy(HMODULE proxy_module) {
    GraphicsPolicy policy = default_policy();

    wchar_t plugin_config_path[MAX_PATH] = {};
    if (!resolve_plugin_config(proxy_module, plugin_config_path)) {
        return policy;
    }

    std::string plugin_config;
    if (!native_aa::read_file(plugin_config_path, &plugin_config)) {
        return policy;
    }

    const std::string manage = photorealism::aa_config::plugin_config_value(
        plugin_config, kNativeGraphicsSection, "manage");
    policy.manage = manage != "false" && manage != "0";
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        policy.desired[index] = policy_value(
            plugin_config, kSettings[index].key, kSettings[index].fallback);
    }
    return policy;
}

}
}
