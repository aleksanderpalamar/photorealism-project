#include "format_names.hpp"

namespace photorealism {
namespace observer {

const char* format_name(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_R32G8X24_TYPELESS:
            return "R32G8X24_TYPELESS";
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            return "D32_FLOAT_S8X24_UINT";
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
            return "R32_FLOAT_X8X24_TYPELESS";
        case DXGI_FORMAT_R32_TYPELESS:
            return "R32_TYPELESS";
        case DXGI_FORMAT_D32_FLOAT:
            return "D32_FLOAT";
        case DXGI_FORMAT_R32_FLOAT:
            return "R32_FLOAT";
        case DXGI_FORMAT_R24G8_TYPELESS:
            return "R24G8_TYPELESS";
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return "D24_UNORM_S8_UINT";
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
            return "R24_UNORM_X8_TYPELESS";
        case DXGI_FORMAT_R16_TYPELESS:
            return "R16_TYPELESS";
        case DXGI_FORMAT_D16_UNORM:
            return "D16_UNORM";
        case DXGI_FORMAT_R16_UNORM:
            return "R16_UNORM";
        default:
            return "OTHER";
    }
}
}
}
