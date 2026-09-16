#include "surface_constants.hpp"

#include <windows.h>

#include <atomic>
#include <cmath>

#include "../postprocess/com_utils.hpp"
#include "../runtime.hpp"

namespace photorealism {
namespace shader_patch {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kRoadMaterialMask = 32.0f;
constexpr UINT kGBufferTargetCount = 4;
constexpr unsigned kNormalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
constexpr unsigned kMaterialFormat = DXGI_FORMAT_R16G16B16A16_UINT;

ID3D11Buffer* g_buffer = nullptr;
SurfaceConstants g_values = {};
std::atomic<bool> g_dirty{true};
std::atomic<unsigned> g_binds{0};
unsigned long long g_origin_ms = 0ull;
bool g_announced = false;

unsigned long long now_ms() { return GetTickCount64(); }

unsigned format_of(ID3D11RenderTargetView* view) {
    if (view == nullptr) {
        return 0;
    }
    D3D11_RENDER_TARGET_VIEW_DESC description = {};
    view->GetDesc(&description);
    return static_cast<unsigned>(description.Format);
}

bool is_gbuffer_bind(
    UINT render_target_count, ID3D11RenderTargetView* const* render_targets) {
    if (render_targets == nullptr ||
        render_target_count != kGBufferTargetCount) {
        return false;
    }
    for (UINT index = 0; index < 3; ++index) {
        if (format_of(render_targets[index]) != kNormalFormat) {
            return false;
        }
    }
    return format_of(render_targets[3]) == kMaterialFormat;
}

}  // namespace

bool create_surface_constants(ID3D11Device* device) {
    release_surface_constants();
    if (device == nullptr) {
        return false;
    }
    D3D11_BUFFER_DESC description = {};
    description.ByteWidth = sizeof(SurfaceConstants);
    description.Usage = D3D11_USAGE_DYNAMIC;
    description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device->CreateBuffer(&description, nullptr, &g_buffer))) {
        g_buffer = nullptr;
        return false;
    }
    g_origin_ms = now_ms();
    g_dirty.store(true, std::memory_order_release);
    return true;
}

void release_surface_constants() {
    safe_release(g_buffer);
    g_binds.store(0, std::memory_order_release);
    g_announced = false;
}

bool surface_constants_ready() { return g_buffer != nullptr; }

unsigned surface_bind_count() {
    return g_binds.load(std::memory_order_acquire);
}

void update_surface_constants(
    const Settings& settings, UINT width, UINT height) {
    const float vertical_fov = settings.depth_vertical_fov * kPi / 180.0f;
    const float y = 1.0f / std::tan(vertical_fov * 0.5f);
    const float aspect = height > 0 ? static_cast<float>(width) /
                                          static_cast<float>(height)
                                    : 1.0f;

    const float seconds =
        static_cast<float>(now_ms() - g_origin_ms) / 1000.0f;

    SurfaceConstants values = {};
    values.rain[0] = settings.wet_roads_amount;
    values.rain[1] = 0.5f + settings.wet_roads_ripple * 3.5f;
    values.rain[2] = seconds * (0.25f + settings.wet_roads_ripple * 0.55f);
    values.rain[3] = settings.wet_surface_enabled ? 1.0f : 0.0f;

    values.road[0] = settings.profile_roads_normal_intensity;
    values.road[1] = settings.profile_roads_default_normals > 0.5f ? 1.0f : 0.35f;
    values.road[2] = settings.wet_roads_gloss;
    values.road[3] = settings.wet_roads_darkening;

    values.frame[0] = y / aspect;
    values.frame[1] = y;
    values.frame[2] = width > 0 ? 1.0f / static_cast<float>(width) : 0.0f;
    values.frame[3] = height > 0 ? 1.0f / static_cast<float>(height) : 0.0f;

    values.mask[0] = kRoadMaterialMask;
    values.mask[1] = settings.wet_roads_floor;
    values.mask[2] = 0.0f;
    values.mask[3] = 0.0f;

    g_values = values;
    g_dirty.store(true, std::memory_order_release);
}

void bind_surface_constants(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets) {
    if (g_buffer == nullptr || context == nullptr) {
        return;
    }
    if (!is_gbuffer_bind(render_target_count, render_targets)) {
        return;
    }

    if (g_dirty.exchange(false, std::memory_order_acq_rel)) {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(context->Map(
                g_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            CopyMemory(mapped.pData, &g_values, sizeof(g_values));
            context->Unmap(g_buffer, 0);
        }
    }

    ID3D11Buffer* buffers[1] = {g_buffer};
    context->PSSetConstantBuffers(kSurfaceConstantSlot, 1, buffers);

    const unsigned count = g_binds.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (!g_announced && count == 1) {
        g_announced = true;
        log_message(
            "Superficie molhada: G-buffer reconhecido (4 alvos f%u/f%u/f%u/f%u); "
            "constantes ligadas em b%u.",
            kNormalFormat,
            kNormalFormat,
            kNormalFormat,
            kMaterialFormat,
            kSurfaceConstantSlot);
    }
}

}
}
