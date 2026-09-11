#include "../runtime.hpp"
#include "capture_gate.hpp"
#include "capture_slots.hpp"
#include "conversion_worker.hpp"
#include "integration.hpp"
#include "steam_screenshots.hpp"

#include <windows.h>

namespace photorealism {
namespace {

using namespace steam;

class FrameDevice {
  public:
    ~FrameDevice() {
        if (context_ != nullptr) {
            context_->Release();
        }
        if (device_ != nullptr) {
            device_->Release();
        }
    }

    bool acquire(IDXGISwapChain* swap_chain) {
        const HRESULT result = swap_chain->GetDevice(
            IID_ID3D11Device, reinterpret_cast<void**>(&device_));
        if (FAILED(result) || device_ == nullptr) {
            return false;
        }
        device_->GetImmediateContext(&context_);
        return context_ != nullptr;
    }

    ID3D11Device* device() const { return device_; }
    ID3D11DeviceContext* context() const { return context_; }

  private:
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
};

void log_write_completed(std::uint32_t handle) {
    const std::uint64_t completed =
        g_completed_writes.fetch_add(1, std::memory_order_relaxed) + 1;
    log_message(
        "Steam screenshot 0.10.4 concluido: result=ok "
        "write_handle=%u writes=%llu accepted=%llu coalesced=%llu.",
        handle,
        static_cast<unsigned long long>(completed),
        static_cast<unsigned long long>(accepted_requests()),
        static_cast<unsigned long long>(coalesced_requests()));
}

bool write_ready_slots() {
    CaptureSlot* slot = find_slot(CaptureState::ready);
    if (slot == nullptr) {
        return true;
    }

    const std::uint32_t size = capture_width() * capture_height() * 3u;
    const std::uint32_t handle = api().write(
        slot->rgb.get(),
        size,
        static_cast<int>(capture_width()),
        static_cast<int>(capture_height()));
    slot->state = CaptureState::empty;
    if (handle == 0u) {
        return false;
    }

    g_capture_cycle_active.store(false, std::memory_order_release);
    log_write_completed(handle);
    return true;
}

void copy_mapped_rows(const D3D11_MAPPED_SUBRESOURCE& mapped, CaptureSlot* slot) {
    const UINT width = capture_width();
    for (UINT row = 0; row < capture_height(); ++row) {
        const auto* source = static_cast<const std::uint8_t*>(mapped.pData) +
                             static_cast<std::size_t>(row) * mapped.RowPitch;
        std::uint8_t* destination =
            slot->raw.get() + static_cast<std::size_t>(row) * width * 4u;
        CopyMemory(destination, source, width * 4u);
    }
}

void drain_finished_readbacks(ID3D11DeviceContext* context) {
    for (CaptureSlot& slot : capture_slots()) {
        const bool arrived =
            slot.state == CaptureState::gpu_pending &&
            context->GetData(slot.completion, nullptr, 0, 0) == S_OK;
        if (!arrived) {
            continue;
        }
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(context->Map(slot.staging, 0, D3D11_MAP_READ, 0, &mapped))) {
            continue;
        }
        copy_mapped_rows(mapped, &slot);
        context->Unmap(slot.staging, 0);
        slot.state = CaptureState::cpu_pending;
        notify_conversion_worker();
    }
}

class BackBuffer {
  public:
    ~BackBuffer() {
        if (texture_ != nullptr) {
            texture_->Release();
        }
    }

    bool acquire(IDXGISwapChain* swap_chain) {
        const HRESULT result = swap_chain->GetBuffer(
            0, IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture_));
        return SUCCEEDED(result) && texture_ != nullptr;
    }

    ID3D11Texture2D* texture() const { return texture_; }

  private:
    ID3D11Texture2D* texture_ = nullptr;
};

bool submit_requested_capture(
    IDXGISwapChain* swap_chain, const FrameDevice& frame) {
    CaptureSlot* available = find_slot(CaptureState::empty);
    if (available == nullptr) {
        return true;
    }

    BackBuffer back_buffer;
    if (!back_buffer.acquire(swap_chain)) {
        return true;
    }

    D3D11_TEXTURE2D_DESC description = {};
    back_buffer.texture()->GetDesc(&description);
    if (!ensure_capture_slots(frame.device(), description)) {
        return false;
    }

    frame.context()->CopyResource(available->staging, back_buffer.texture());
    frame.context()->End(available->completion);
    available->state = CaptureState::gpu_pending;
    g_requests.fetch_sub(1u, std::memory_order_acq_rel);
    return true;
}

bool context_is_needed() {
    return find_slot(CaptureState::gpu_pending) != nullptr ||
           g_requests.load(std::memory_order_acquire) != 0;
}

}

void observe_postprocessed_frame(IDXGISwapChain* swap_chain) {
    if (swap_chain == nullptr || !steam::ensure_integration()) {
        return;
    }

    CaptureLock lock;
    if (!lock.held()) {
        return;
    }

    if (!write_ready_slots()) {
        lock.release();
        steam::return_to_native_capture("WriteScreenshot-failed", true);
        return;
    }
    if (!context_is_needed()) {
        return;
    }

    FrameDevice frame;
    if (!frame.acquire(swap_chain)) {
        return;
    }

    drain_finished_readbacks(frame.context());
    if (g_requests.load(std::memory_order_acquire) == 0) {
        return;
    }
    if (submit_requested_capture(swap_chain, frame)) {
        return;
    }
    lock.release();
    steam::return_to_native_capture("unsupported-backbuffer", true);
}

}
