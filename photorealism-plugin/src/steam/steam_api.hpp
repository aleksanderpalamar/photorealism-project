#pragma once

#include <cstdint>

namespace photorealism {
namespace steam {

constexpr int kScreenshotRequestedCallback = 2302;

class CallbackBase {
  public:
    virtual void Run(void* parameter) = 0;
    virtual void Run(
        void* parameter, bool io_failure, std::uint64_t api_call) = 0;
    virtual int GetCallbackSizeBytes() = 0;

    std::uint8_t flags = 0;
    std::uint8_t padding[3] = {};
    int callback = 0;
};

struct ScreenshotsApi {
    void* screenshots;
    void (*hook_screenshots)(void*, bool);
    bool (*is_hooked)(void*);
    std::uint32_t (*write_screenshot)(void*, void*, std::uint32_t, int, int);
    void (*trigger_screenshot)(void*);
    void (*register_callback)(CallbackBase*, int);
    void (*unregister_callback)(CallbackBase*);

    bool complete() const;
    void hook(bool enabled) const;
    void trigger() const;
    std::uint32_t write(
        void* rgb, std::uint32_t size, int width, int height) const;
};

enum class LoadResult {
    module_absent,
    exports_incomplete,
    ready,
};

LoadResult load_screenshots_api(ScreenshotsApi* api);

}
}
