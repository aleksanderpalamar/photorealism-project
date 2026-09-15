#include "dds_header.hpp"

namespace photorealism {
namespace frame_capture {
namespace {

constexpr std::uint32_t kFlagsCaps = 0x1;
constexpr std::uint32_t kFlagsHeight = 0x2;
constexpr std::uint32_t kFlagsWidth = 0x4;
constexpr std::uint32_t kFlagsPixelFormat = 0x1000;
constexpr std::uint32_t kPixelFormatFourCc = 0x4;
constexpr std::uint32_t kCapsTexture = 0x1000;
constexpr std::uint32_t kDimensionTexture2d = 3;

struct FormatRange {
    unsigned first;
    unsigned last;
    unsigned bytes;
};

constexpr FormatRange kFormatBytes[] = {
    {1, 4, 16}, {5, 8, 12}, {9, 14, 8}, {15, 22, 8}, {23, 32, 4},
    {33, 47, 4}, {48, 52, 2}, {53, 59, 2}, {60, 65, 1}, {85, 86, 2},
    {87, 93, 4}, {115, 115, 2},
};

void put(std::array<std::uint8_t, kDdsHeaderBytes>* bytes, std::size_t offset,
         std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        (*bytes)[offset + index] = static_cast<std::uint8_t>(value >> (8 * index));
    }
}

}

unsigned bytes_per_pixel(unsigned dxgi_format) {
    for (const FormatRange& range : kFormatBytes) {
        if (dxgi_format >= range.first && dxgi_format <= range.last) {
            return range.bytes;
        }
    }
    return 0;
}

std::array<std::uint8_t, kDdsHeaderBytes> dds_header(
    unsigned width, unsigned height, unsigned dxgi_format) {
    std::array<std::uint8_t, kDdsHeaderBytes> bytes = {};
    put(&bytes, 0, 0x20534444);
    put(&bytes, 4, 124);
    put(&bytes, 8, kFlagsCaps | kFlagsHeight | kFlagsWidth | kFlagsPixelFormat);
    put(&bytes, 12, height);
    put(&bytes, 16, width);
    put(&bytes, 20, width * height * bytes_per_pixel(dxgi_format));
    put(&bytes, 28, 1);
    put(&bytes, 76, 32);
    put(&bytes, 80, kPixelFormatFourCc);
    put(&bytes, 84, 0x30315844);
    put(&bytes, 108, kCapsTexture);
    put(&bytes, 128, dxgi_format);
    put(&bytes, 132, kDimensionTexture2d);
    put(&bytes, 140, 1);
    return bytes;
}

}
}
