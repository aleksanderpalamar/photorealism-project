#pragma once

#include <d3d11.h>
#include <windows.h>

#include <array>
#include <cstdint>
#include <memory>

namespace photorealism {
namespace steam {

constexpr std::size_t kCaptureSlotCount = 2;

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

extern SRWLOCK g_capture_lock;

class CaptureLock {
  public:
    CaptureLock() : held_(TryAcquireSRWLockExclusive(&g_capture_lock) != 0) {}
    ~CaptureLock() { release(); }

    CaptureLock(const CaptureLock&) = delete;
    CaptureLock& operator=(const CaptureLock&) = delete;

    bool held() const { return held_; }

    void release() {
        if (held_) {
            ReleaseSRWLockExclusive(&g_capture_lock);
            held_ = false;
        }
    }

  private:
    bool held_;
};

class BlockingCaptureLock {
  public:
    BlockingCaptureLock() { AcquireSRWLockExclusive(&g_capture_lock); }
    ~BlockingCaptureLock() { ReleaseSRWLockExclusive(&g_capture_lock); }

    BlockingCaptureLock(const BlockingCaptureLock&) = delete;
    BlockingCaptureLock& operator=(const BlockingCaptureLock&) = delete;
};

std::array<CaptureSlot, kCaptureSlotCount>& capture_slots();
CaptureSlot* find_slot(CaptureState state);

UINT capture_width();
UINT capture_height();
bool capture_is_bgra();

bool ensure_capture_slots(
    ID3D11Device* device, const D3D11_TEXTURE2D_DESC& source);
void release_capture_slots_locked();

}
}
