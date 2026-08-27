#pragma once

#include <cstdint>

// Rotulos legiveis para os enums DXGI e D3D que aparecem no log de diagnostico.
//
// Deliberadamente livre de Win32/D3D: os valores numericos sao os do DXGI e do
// D3D_FEATURE_LEVEL, presos aos originais por static_assert em fsr_module.cpp.
namespace photorealism::fsr {

constexpr std::uint32_t kFormatR16G16B16A16Float = 10;
constexpr std::uint32_t kFormatR10G10B10A2Unorm = 24;
constexpr std::uint32_t kFormatR11G11B10Float = 26;
constexpr std::uint32_t kFormatR8G8B8A8Unorm = 28;
constexpr std::uint32_t kFormatR8G8B8A8UnormSrgb = 29;
constexpr std::uint32_t kFormatB8G8R8A8Unorm = 87;
constexpr std::uint32_t kFormatB8G8R8A8UnormSrgb = 91;

constexpr const char* dxgi_format_name(std::uint32_t format) {
    switch (format) {
        case kFormatR16G16B16A16Float:
            return "R16G16B16A16_FLOAT";
        case kFormatR11G11B10Float:
            return "R11G11B10_FLOAT";
        case kFormatR10G10B10A2Unorm:
            return "R10G10B10A2_UNORM";
        case kFormatR8G8B8A8Unorm:
            return "R8G8B8A8_UNORM";
        case kFormatR8G8B8A8UnormSrgb:
            return "R8G8B8A8_UNORM_SRGB";
        case kFormatB8G8R8A8Unorm:
            return "B8G8R8A8_UNORM";
        case kFormatB8G8R8A8UnormSrgb:
            return "B8G8R8A8_UNORM_SRGB";
        default:
            return "other";
    }
}

constexpr std::uint32_t kFeatureLevel9_1 = 0x9100;
constexpr std::uint32_t kFeatureLevel9_2 = 0x9200;
constexpr std::uint32_t kFeatureLevel9_3 = 0x9300;
constexpr std::uint32_t kFeatureLevel10_0 = 0xA000;
constexpr std::uint32_t kFeatureLevel10_1 = 0xA100;
constexpr std::uint32_t kFeatureLevel11_0 = 0xB000;
constexpr std::uint32_t kFeatureLevel11_1 = 0xB100;
constexpr std::uint32_t kFeatureLevel12_0 = 0xC000;
constexpr std::uint32_t kFeatureLevel12_1 = 0xC100;

constexpr const char* feature_level_name(std::uint32_t level) {
    switch (level) {
        case kFeatureLevel12_1:
            return "12_1";
        case kFeatureLevel12_0:
            return "12_0";
        case kFeatureLevel11_1:
            return "11_1";
        case kFeatureLevel11_0:
            return "11_0";
        case kFeatureLevel10_1:
            return "10_1";
        case kFeatureLevel10_0:
            return "10_0";
        case kFeatureLevel9_3:
            return "9_3";
        case kFeatureLevel9_2:
            return "9_2";
        case kFeatureLevel9_1:
            return "9_1";
        default:
            return "unknown";
    }
}

}  // namespace photorealism::fsr
