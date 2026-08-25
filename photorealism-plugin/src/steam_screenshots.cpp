#include "steam_screenshots.hpp"

#include "runtime.hpp"
#include "screenshot_request_gate.hpp"

#include <d3d11.h>
#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace photorealism {
namespace {

constexpr int kScreenshotRequestedCallback = 2302;
constexpr std::size_t kCaptureSlotCount = 2;

class SteamCallbackBase {
public:
    virtual void Run(void* parameter) = 0;
    virtual void Run(
        void* parameter, bool io_failure, std::uint64_t api_call) = 0;
    virtual int GetCallbackSizeBytes() = 0;

    std::uint8_t flags = 0;
    std::uint8_t padding[3] = {};
    int callback = 0;
};

using SteamScreenshotsFunction = void* (*)();
using HookScreenshotsFunction = void (*)(void*, bool);
using IsScreenshotsHookedFunction = bool (*)(void*);
using WriteScreenshotFunction = std::uint32_t (*)(
    void*, void*, std::uint32_t, int, int);
using TriggerScreenshotFunction = void (*)(void*);
using RegisterCallbackFunction = void (*)(SteamCallbackBase*, int);
using UnregisterCallbackFunction = void (*)(SteamCallbackBase*);

enum class CaptureState : std::uint8_t {
    empty,
    gpu_pending,
    cpu_pending,
    converting,
    ready,
};

struct CaptureSlot {
    CaptureState state = CaptureState::empty;
    ID3D11Texture2D* staging = nullptr;
    ID3D11Query* completion = nullptr;
    std::unique_ptr<std::uint8_t[]> raw;
    std::unique_ptr<std::uint8_t[]> rgb;
};

std::atomic<std::uint32_t> g_requests{0};
std::atomic<bool> g_callback_accepting{false};
std::atomic<bool> g_capture_cycle_active{false};
std::atomic<std::uint64_t> g_last_accepted_tick{0};
std::atomic<std::uint64_t> g_accepted_requests{0};
std::atomic<std::uint64_t> g_coalesced_requests{0};
std::atomic<std::uint64_t> g_completed_writes{0};
SRWLOCK g_capture_lock = SRWLOCK_INIT;
std::array<CaptureSlot, kCaptureSlotCount> g_slots = {};
HANDLE g_worker_event = nullptr;
HANDLE g_worker_stop_event = nullptr;
HANDLE g_worker = nullptr;
HMODULE g_steam_module = nullptr;
void* g_screenshots = nullptr;
HookScreenshotsFunction g_hook_screenshots = nullptr;
IsScreenshotsHookedFunction g_is_hooked = nullptr;
WriteScreenshotFunction g_write_screenshot = nullptr;
TriggerScreenshotFunction g_trigger_screenshot = nullptr;
RegisterCallbackFunction g_register_callback = nullptr;
UnregisterCallbackFunction g_unregister_callback = nullptr;
UINT g_width = 0;
UINT g_height = 0;
DXGI_FORMAT g_format = DXGI_FORMAT_UNKNOWN;
bool g_integration_attempted = false;
unsigned g_integration_attempt_count = 0;
ULONGLONG g_last_integration_attempt = 0;
bool g_callback_registered = false;
bool g_custom_capture_active = false;
bool g_custom_capture_failed = false;

template <typename T>
void release(T*& object) {
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

class ScreenshotRequestedCallback final : public SteamCallbackBase {
public:
    ScreenshotRequestedCallback() {
        callback = kScreenshotRequestedCallback;
    }

    void Run(void*) override {
        if (!g_callback_accepting.load(std::memory_order_acquire)) {
            return;
        }
        const std::uint64_t now = GetTickCount64();
        bool expected = false;
        if (!g_capture_cycle_active.compare_exchange_strong(
                expected,
                true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            g_coalesced_requests.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const std::uint64_t previous =
            g_last_accepted_tick.load(std::memory_order_acquire);
        if (steam_screenshot::request_is_duplicate(now, previous, false)) {
            g_capture_cycle_active.store(false, std::memory_order_release);
            g_coalesced_requests.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        g_last_accepted_tick.store(now, std::memory_order_release);
        g_accepted_requests.fetch_add(1, std::memory_order_relaxed);
        g_requests.store(1u, std::memory_order_release);
    }

    // ScreenshotRequested_t is a normal callback, not an API-call result.
    // Accepting this second virtual entry as another request can duplicate one
    // physical F12 delivery on compatibility layers.
    void Run(void*, bool, std::uint64_t) override {}

    int GetCallbackSizeBytes() override { return 1; }
};

ScreenshotRequestedCallback g_screenshot_callback;
static_assert(
    sizeof(ScreenshotRequestedCallback) == 16,
    "Steam CCallbackBase ABI layout changed for Windows x64");

bool supported_capture_format(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_R8G8B8A8_UNORM ||
           format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
           format == DXGI_FORMAT_B8G8R8A8_UNORM ||
           format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
}

void release_capture_slots_locked() {
    for (CaptureSlot& slot : g_slots) {
        release(slot.completion);
        release(slot.staging);
        slot.raw.reset();
        slot.rgb.reset();
        slot.state = CaptureState::empty;
    }
    g_width = 0;
    g_height = 0;
    g_format = DXGI_FORMAT_UNKNOWN;
}

DWORD WINAPI conversion_worker(LPVOID) {
    for (;;) {
        HANDLE events[2] = {g_worker_stop_event, g_worker_event};
        const DWORD wait = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) {
            return 0;
        }
        if (wait != WAIT_OBJECT_0 + 1) {
            return 1;
        }
        bool found = true;
        while (found) {
            found = false;
            AcquireSRWLockExclusive(&g_capture_lock);
            for (CaptureSlot& slot : g_slots) {
                if (slot.state != CaptureState::cpu_pending ||
                    slot.raw == nullptr || slot.rgb == nullptr) {
                    continue;
                }
                found = true;
                slot.state = CaptureState::converting;
                const bool bgra =
                    g_format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                    g_format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                const std::size_t pixels =
                    static_cast<std::size_t>(g_width) * g_height;
                for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
                    const std::uint8_t* source = &slot.raw[pixel * 4u];
                    std::uint8_t* destination = &slot.rgb[pixel * 3u];
                    destination[0] = source[bgra ? 2u : 0u];
                    destination[1] = source[1];
                    destination[2] = source[bgra ? 0u : 2u];
                }
                slot.state = CaptureState::ready;
                break;
            }
            ReleaseSRWLockExclusive(&g_capture_lock);
        }
    }
}

bool start_worker() {
    if (g_worker != nullptr) {
        return true;
    }
    g_worker_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_worker_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_worker_event == nullptr || g_worker_stop_event == nullptr) {
        if (g_worker_event != nullptr) {
            CloseHandle(g_worker_event);
        }
        if (g_worker_stop_event != nullptr) {
            CloseHandle(g_worker_stop_event);
        }
        g_worker_event = nullptr;
        g_worker_stop_event = nullptr;
        return false;
    }
    g_worker = CreateThread(nullptr, 0, conversion_worker, nullptr, 0, nullptr);
    if (g_worker == nullptr) {
        CloseHandle(g_worker_event);
        CloseHandle(g_worker_stop_event);
        g_worker_event = nullptr;
        g_worker_stop_event = nullptr;
        return false;
    }
    return true;
}

template <typename Function>
Function steam_export(const char* name) {
    return reinterpret_cast<Function>(GetProcAddress(g_steam_module, name));
}

bool initialize_steam_screenshots() {
    if (g_custom_capture_active) {
        return true;
    }
    if (g_integration_attempted || g_custom_capture_failed) {
        return false;
    }
    const ULONGLONG now = GetTickCount64();
    if (g_last_integration_attempt != 0 &&
        now - g_last_integration_attempt < 1000u) {
        return false;
    }
    g_last_integration_attempt = now;
    ++g_integration_attempt_count;
    g_steam_module = GetModuleHandleW(L"steam_api64.dll");
    if (g_steam_module == nullptr) {
        if (g_integration_attempt_count >= 30u) {
            g_integration_attempted = true;
            log_message(
                "Steam screenshot 0.10.4 indisponivel apos espera limitada; "
                "overlay nativo permanece responsavel pelo F12.");
        }
        return false;
    }
    const SteamScreenshotsFunction screenshots =
        steam_export<SteamScreenshotsFunction>("SteamAPI_SteamScreenshots_v003");
    g_hook_screenshots = steam_export<HookScreenshotsFunction>(
        "SteamAPI_ISteamScreenshots_HookScreenshots");
    g_is_hooked = steam_export<IsScreenshotsHookedFunction>(
        "SteamAPI_ISteamScreenshots_IsScreenshotsHooked");
    g_write_screenshot = steam_export<WriteScreenshotFunction>(
        "SteamAPI_ISteamScreenshots_WriteScreenshot");
    g_trigger_screenshot = steam_export<TriggerScreenshotFunction>(
        "SteamAPI_ISteamScreenshots_TriggerScreenshot");
    g_register_callback = steam_export<RegisterCallbackFunction>(
        "SteamAPI_RegisterCallback");
    g_unregister_callback = steam_export<UnregisterCallbackFunction>(
        "SteamAPI_UnregisterCallback");
    g_screenshots = screenshots != nullptr ? screenshots() : nullptr;
    if (g_screenshots == nullptr || g_hook_screenshots == nullptr ||
        g_write_screenshot == nullptr || g_trigger_screenshot == nullptr ||
        g_register_callback == nullptr || g_unregister_callback == nullptr) {
        if (g_integration_attempt_count >= 30u || screenshots == nullptr ||
            g_hook_screenshots == nullptr || g_write_screenshot == nullptr ||
            g_trigger_screenshot == nullptr || g_register_callback == nullptr ||
            g_unregister_callback == nullptr) {
            g_integration_attempted = true;
            log_message(
                "Steamworks screenshots v003 incompleto ou ainda nao "
                "inicializado; HookScreenshots nao ativado e F12 nativo "
                "preservado.");
        }
        return false;
    }
    if (!start_worker()) {
        g_integration_attempted = true;
        log_message(
            "Worker de screenshot Steam indisponivel; HookScreenshots nao "
            "ativado e F12 nativo preservado.");
        return false;
    }
    g_register_callback(
        &g_screenshot_callback, kScreenshotRequestedCallback);
    g_callback_registered = true;
    g_hook_screenshots(g_screenshots, true);
    if (g_is_hooked != nullptr && !g_is_hooked(g_screenshots)) {
        g_unregister_callback(&g_screenshot_callback);
        g_callback_registered = false;
        log_message(
            "Steam recusou HookScreenshots; F12 nativo preservado.");
        return false;
    }
    g_callback_accepting.store(true, std::memory_order_release);
    g_custom_capture_active = true;
    g_integration_attempted = true;
    log_message(
        "Steam screenshot 0.10.4 ativo via ISteamScreenshots v003: "
        "ScreenshotRequested_t=2302, readback assincrono e exatamente um "
        "WriteScreenshot por toque; deduplicacao=750ms e nenhuma tecla "
        "interceptada.");
    return true;
}

void return_to_native_capture(const char* reason, bool trigger) {
    if (!g_custom_capture_active) {
        return;
    }
    g_callback_accepting.store(false, std::memory_order_release);
    g_hook_screenshots(g_screenshots, false);
    if (g_callback_registered) {
        g_unregister_callback(&g_screenshot_callback);
        g_callback_registered = false;
    }
    g_custom_capture_active = false;
    g_custom_capture_failed = true;
    g_requests.store(0, std::memory_order_release);
    g_capture_cycle_active.store(false, std::memory_order_release);
    log_message(
        "Steam screenshot 0.10.4 retornou ao overlay nativo: result=failure "
        "reason=%s accepted=%llu coalesced=%llu.",
        reason,
        static_cast<unsigned long long>(
            g_accepted_requests.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_coalesced_requests.load(std::memory_order_relaxed)));
    if (trigger && g_trigger_screenshot != nullptr) {
        g_trigger_screenshot(g_screenshots);
    }
    shutdown_steam_screenshots();
}

bool ensure_capture_slots(
    ID3D11Device* device, const D3D11_TEXTURE2D_DESC& source) {
    if (g_width == source.Width && g_height == source.Height &&
        g_format == source.Format && g_slots[0].staging != nullptr) {
        return true;
    }
    release_capture_slots_locked();
    if (!supported_capture_format(source.Format) ||
        source.SampleDesc.Count != 1) {
        return false;
    }
    const std::size_t pixels =
        static_cast<std::size_t>(source.Width) * source.Height;
    D3D11_TEXTURE2D_DESC staging = source;
    staging.Usage = D3D11_USAGE_STAGING;
    staging.BindFlags = 0;
    staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging.MiscFlags = 0;
    D3D11_QUERY_DESC query = {D3D11_QUERY_EVENT, 0};
    for (CaptureSlot& slot : g_slots) {
        slot.raw.reset(new (std::nothrow) std::uint8_t[pixels * 4u]);
        slot.rgb.reset(new (std::nothrow) std::uint8_t[pixels * 3u]);
        if (slot.raw == nullptr || slot.rgb == nullptr ||
            FAILED(device->CreateTexture2D(
                &staging, nullptr, &slot.staging)) ||
            FAILED(device->CreateQuery(&query, &slot.completion))) {
            release_capture_slots_locked();
            return false;
        }
    }
    g_width = source.Width;
    g_height = source.Height;
    g_format = source.Format;
    return true;
}

}  // namespace

void observe_postprocessed_frame(IDXGISwapChain* swap_chain) {
    if (swap_chain == nullptr || !initialize_steam_screenshots() ||
        !TryAcquireSRWLockExclusive(&g_capture_lock)) {
        return;
    }
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;

    for (CaptureSlot& slot : g_slots) {
        if (slot.state == CaptureState::ready) {
            const std::uint32_t handle = g_write_screenshot(
                g_screenshots,
                slot.rgb.get(),
                g_width * g_height * 3u,
                static_cast<int>(g_width),
                static_cast<int>(g_height));
            slot.state = CaptureState::empty;
            if (handle == 0u) {
                ReleaseSRWLockExclusive(&g_capture_lock);
                return_to_native_capture("WriteScreenshot-failed", true);
                return;
            }
            const std::uint64_t completed =
                g_completed_writes.fetch_add(1, std::memory_order_relaxed) + 1;
            g_capture_cycle_active.store(false, std::memory_order_release);
            log_message(
                "Steam screenshot 0.10.4 concluido: result=ok "
                "write_handle=%u writes=%llu accepted=%llu coalesced=%llu.",
                handle,
                static_cast<unsigned long long>(completed),
                static_cast<unsigned long long>(
                    g_accepted_requests.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(
                    g_coalesced_requests.load(std::memory_order_relaxed)));
        }
    }

    bool needs_context = false;
    for (const CaptureSlot& slot : g_slots) {
        needs_context = needs_context || slot.state == CaptureState::gpu_pending;
    }
    const bool has_request = g_requests.load(std::memory_order_acquire) != 0;
    if (needs_context || has_request) {
        if (FAILED(swap_chain->GetDevice(
                IID_ID3D11Device, reinterpret_cast<void**>(&device))) ||
            device == nullptr) {
            ReleaseSRWLockExclusive(&g_capture_lock);
            return;
        }
        device->GetImmediateContext(&context);
    }

    if (context != nullptr) {
        for (CaptureSlot& slot : g_slots) {
            if (slot.state != CaptureState::gpu_pending ||
                context->GetData(slot.completion, nullptr, 0, 0) != S_OK) {
                continue;
            }
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (FAILED(context->Map(
                    slot.staging, 0, D3D11_MAP_READ, 0, &mapped))) {
                continue;
            }
            for (UINT row = 0; row < g_height; ++row) {
                const auto* source = static_cast<const std::uint8_t*>(mapped.pData) +
                                     static_cast<std::size_t>(row) * mapped.RowPitch;
                std::uint8_t* destination = slot.raw.get() +
                    static_cast<std::size_t>(row) * g_width * 4u;
                CopyMemory(destination, source, g_width * 4u);
            }
            context->Unmap(slot.staging, 0);
            slot.state = CaptureState::cpu_pending;
            SetEvent(g_worker_event);
        }
    }

    if (has_request && context != nullptr) {
        CaptureSlot* available = nullptr;
        for (CaptureSlot& slot : g_slots) {
            if (slot.state == CaptureState::empty) {
                available = &slot;
                break;
            }
        }
        if (available != nullptr) {
            ID3D11Texture2D* backbuffer = nullptr;
            if (SUCCEEDED(swap_chain->GetBuffer(
                    0,
                    IID_ID3D11Texture2D,
                    reinterpret_cast<void**>(&backbuffer))) &&
                backbuffer != nullptr) {
                D3D11_TEXTURE2D_DESC description = {};
                backbuffer->GetDesc(&description);
                if (ensure_capture_slots(device, description)) {
                    context->CopyResource(available->staging, backbuffer);
                    context->End(available->completion);
                    available->state = CaptureState::gpu_pending;
                    g_requests.fetch_sub(1u, std::memory_order_acq_rel);
                } else {
                    release(backbuffer);
                    release(context);
                    release(device);
                    ReleaseSRWLockExclusive(&g_capture_lock);
                    return_to_native_capture("unsupported-backbuffer", true);
                    return;
                }
                release(backbuffer);
            }
        }
    }
    release(context);
    release(device);
    ReleaseSRWLockExclusive(&g_capture_lock);
}

void prepare_steam_screenshot_resize() {
    shutdown_steam_screenshots();
}

void shutdown_steam_screenshots() {
    g_callback_accepting.store(false, std::memory_order_release);
    if (g_custom_capture_active && g_hook_screenshots != nullptr &&
        g_screenshots != nullptr) {
        g_hook_screenshots(g_screenshots, false);
    }
    if (g_callback_registered && g_unregister_callback != nullptr) {
        g_unregister_callback(&g_screenshot_callback);
        g_callback_registered = false;
    }
    g_custom_capture_active = false;
    g_requests.store(0, std::memory_order_release);
    g_capture_cycle_active.store(false, std::memory_order_release);
    HANDLE worker = g_worker;
    if (g_worker_stop_event != nullptr) {
        SetEvent(g_worker_stop_event);
    }
    if (g_worker_event != nullptr) {
        SetEvent(g_worker_event);
    }
    if (worker != nullptr) {
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
    }
    g_worker = nullptr;
    if (g_worker_event != nullptr) {
        CloseHandle(g_worker_event);
        g_worker_event = nullptr;
    }
    if (g_worker_stop_event != nullptr) {
        CloseHandle(g_worker_stop_event);
        g_worker_stop_event = nullptr;
    }
    AcquireSRWLockExclusive(&g_capture_lock);
    release_capture_slots_locked();
    ReleaseSRWLockExclusive(&g_capture_lock);
    if (!g_custom_capture_failed) {
        g_integration_attempted = false;
        g_integration_attempt_count = 0;
        g_last_integration_attempt = 0;
    }
}

}  // namespace photorealism
