#include "steam_api.hpp"

#include <windows.h>

namespace photorealism {
namespace steam {
namespace {

using SteamScreenshotsFunction = void* (*)();

template <typename Function>
Function steam_export(HMODULE module, const char* name) {
    return reinterpret_cast<Function>(GetProcAddress(module, name));
}

}

bool ScreenshotsApi::complete() const {
    return screenshots != nullptr && hook_screenshots != nullptr &&
           write_screenshot != nullptr && trigger_screenshot != nullptr &&
           register_callback != nullptr && unregister_callback != nullptr;
}

void ScreenshotsApi::hook(bool enabled) const {
    if (hook_screenshots == nullptr || screenshots == nullptr) {
        return;
    }
    hook_screenshots(screenshots, enabled);
}

void ScreenshotsApi::trigger() const {
    if (trigger_screenshot == nullptr || screenshots == nullptr) {
        return;
    }
    trigger_screenshot(screenshots);
}

std::uint32_t ScreenshotsApi::write(
    void* rgb, std::uint32_t size, int width, int height) const {
    return write_screenshot(screenshots, rgb, size, width, height);
}

LoadResult load_screenshots_api(ScreenshotsApi* api) {
    HMODULE module = GetModuleHandleW(L"steam_api64.dll");
    if (module == nullptr) {
        return LoadResult::module_absent;
    }

    const auto screenshots = steam_export<SteamScreenshotsFunction>(
        module, "SteamAPI_SteamScreenshots_v003");
    api->hook_screenshots = steam_export<void (*)(void*, bool)>(
        module, "SteamAPI_ISteamScreenshots_HookScreenshots");
    api->is_hooked = steam_export<bool (*)(void*)>(
        module, "SteamAPI_ISteamScreenshots_IsScreenshotsHooked");
    api->write_screenshot =
        steam_export<std::uint32_t (*)(void*, void*, std::uint32_t, int, int)>(
            module, "SteamAPI_ISteamScreenshots_WriteScreenshot");
    api->trigger_screenshot = steam_export<void (*)(void*)>(
        module, "SteamAPI_ISteamScreenshots_TriggerScreenshot");
    api->register_callback = steam_export<void (*)(CallbackBase*, int)>(
        module, "SteamAPI_RegisterCallback");
    api->unregister_callback = steam_export<void (*)(CallbackBase*)>(
        module, "SteamAPI_UnregisterCallback");
    api->screenshots = screenshots != nullptr ? screenshots() : nullptr;

    const bool exports_resolved =
        screenshots != nullptr && api->hook_screenshots != nullptr &&
        api->write_screenshot != nullptr && api->trigger_screenshot != nullptr &&
        api->register_callback != nullptr && api->unregister_callback != nullptr;
    if (!exports_resolved) {
        return LoadResult::exports_incomplete;
    }
    return api->screenshots != nullptr ? LoadResult::ready
                                       : LoadResult::exports_incomplete;
}

}
}
