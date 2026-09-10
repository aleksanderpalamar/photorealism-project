#pragma once

#include <dxgiformat.h>

namespace photorealism {

inline bool is_unorm_format(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_B8G8R8A8_UNORM ||
           format == DXGI_FORMAT_R8G8B8A8_UNORM;
}

inline DXGI_FORMAT typeless_format(DXGI_FORMAT format) {
    if (format == DXGI_FORMAT_B8G8R8A8_UNORM ||
        format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
        return DXGI_FORMAT_B8G8R8A8_TYPELESS;
    }
    return DXGI_FORMAT_R8G8B8A8_TYPELESS;
}

inline DXGI_FORMAT srgb_view_format(DXGI_FORMAT format) {
    if (format == DXGI_FORMAT_B8G8R8A8_UNORM ||
        format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    }
    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}

inline bool is_supported_format(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_B8G8R8A8_UNORM ||
           format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
           format == DXGI_FORMAT_R8G8B8A8_UNORM ||
           format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
}

inline bool depth_copy_formats(
    DXGI_FORMAT source,
    DXGI_FORMAT* resource_format,
    DXGI_FORMAT* view_format) {
    if (resource_format == nullptr || view_format == nullptr) {
        return false;
    }

    if (source == DXGI_FORMAT_D32_FLOAT_S8X24_UINT ||
        source == DXGI_FORMAT_R32G8X24_TYPELESS) {
        *resource_format = DXGI_FORMAT_R32G8X24_TYPELESS;
        *view_format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        return true;
    }
    if (source == DXGI_FORMAT_D32_FLOAT ||
        source == DXGI_FORMAT_R32_TYPELESS) {
        *resource_format = DXGI_FORMAT_R32_TYPELESS;
        *view_format = DXGI_FORMAT_R32_FLOAT;
        return true;
    }
    if (source == DXGI_FORMAT_D24_UNORM_S8_UINT ||
        source == DXGI_FORMAT_R24G8_TYPELESS) {
        *resource_format = DXGI_FORMAT_R24G8_TYPELESS;
        *view_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        return true;
    }
    if (source == DXGI_FORMAT_D16_UNORM ||
        source == DXGI_FORMAT_R16_TYPELESS) {
        *resource_format = DXGI_FORMAT_R16_TYPELESS;
        *view_format = DXGI_FORMAT_R16_UNORM;
        return true;
    }
    return false;
}

}  // namespace photorealism
