#include "capture_slots.hpp"

namespace photorealism {
namespace steam {
namespace {

std::array<CaptureSlot, kCaptureSlotCount> g_slots = {};
UINT g_width = 0;
UINT g_height = 0;
DXGI_FORMAT g_format = DXGI_FORMAT_UNKNOWN;

template <typename T>
void release(T*& object) {
    if (object == nullptr) {
        return;
    }
    object->Release();
    object = nullptr;
}

bool supported_capture_format(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_R8G8B8A8_UNORM ||
           format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
           format == DXGI_FORMAT_B8G8R8A8_UNORM ||
           format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
}

bool allocate_slot(
    ID3D11Device* device,
    const D3D11_TEXTURE2D_DESC& staging,
    std::size_t pixels,
    CaptureSlot* slot) {
    slot->raw.reset(new (std::nothrow) std::uint8_t[pixels * 4u]);
    slot->rgb.reset(new (std::nothrow) std::uint8_t[pixels * 3u]);
    if (slot->raw == nullptr || slot->rgb == nullptr) {
        return false;
    }
    D3D11_QUERY_DESC query = {D3D11_QUERY_EVENT, 0};
    return SUCCEEDED(
               device->CreateTexture2D(&staging, nullptr, &slot->staging)) &&
           SUCCEEDED(device->CreateQuery(&query, &slot->completion));
}

}

SRWLOCK g_capture_lock = SRWLOCK_INIT;

std::array<CaptureSlot, kCaptureSlotCount>& capture_slots() {
    return g_slots;
}

CaptureSlot* find_slot(CaptureState state) {
    for (CaptureSlot& slot : g_slots) {
        if (slot.state == state) {
            return &slot;
        }
    }
    return nullptr;
}

UINT capture_width() {
    return g_width;
}

UINT capture_height() {
    return g_height;
}

bool capture_is_bgra() {
    return g_format == DXGI_FORMAT_B8G8R8A8_UNORM ||
           g_format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
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

bool ensure_capture_slots(
    ID3D11Device* device, const D3D11_TEXTURE2D_DESC& source) {
    const bool matches = g_width == source.Width && g_height == source.Height &&
                         g_format == source.Format &&
                         g_slots[0].staging != nullptr;
    if (matches) {
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

    for (CaptureSlot& slot : g_slots) {
        if (allocate_slot(device, staging, pixels, &slot)) {
            continue;
        }
        release_capture_slots_locked();
        return false;
    }

    g_width = source.Width;
    g_height = source.Height;
    g_format = source.Format;
    return true;
}

}
}
