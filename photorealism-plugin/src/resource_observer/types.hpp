#pragma once

#include <d3d11.h>
#include <windows.h>

#include <atomic>
#include <cstdint>

namespace photorealism {
namespace observer {

constexpr UINT kMaximumObservedResources = 256;
constexpr UINT kViewCacheCapacity = 4096;
constexpr UINT kViewCacheProbeCount = 32;
constexpr UINT kMaximumReportedGroups = 12;
constexpr UINT kMaximumReportedResources = 8;
constexpr UINT kInvalidResourceIndex = 0xFFFFFFFFu;
constexpr ULONGLONG kDiscoveryDurationMilliseconds = 30000;
constexpr ULONGLONG kEarlyDiscoveryMinimumMilliseconds = 3000;
struct ObservedDepthResource {
    void* identity;
    ID3D11Texture2D* texture;
    D3D11_TEXTURE2D_DESC description;
    UINT width;
    UINT height;
    DXGI_FORMAT texture_format;
    DXGI_FORMAT view_format;
    UINT sample_count;
    UINT bind_flags;
    volatile LONG64 bindings;
    UINT observed_views;
    UINT generation;
};

struct ViewCacheEntry {
    ID3D11DepthStencilView* identity;
    UINT resource_index;
    UINT resource_generation;
};

struct ResourceSnapshot {
    void* identity;
    ID3D11Texture2D* texture;
    D3D11_TEXTURE2D_DESC description;
    UINT width;
    UINT height;
    DXGI_FORMAT texture_format;
    DXGI_FORMAT view_format;
    UINT sample_count;
    UINT bind_flags;
    std::uint64_t bindings;
    UINT observed_views;
    std::uint64_t score;
};

struct ResolutionGroup {
    UINT width;
    UINT height;
    DXGI_FORMAT texture_format;
    DXGI_FORMAT view_format;
    UINT sample_count;
    UINT bind_flags;
    UINT resource_count;
    std::uint64_t bindings;
    std::uint64_t score;
};

}
}
