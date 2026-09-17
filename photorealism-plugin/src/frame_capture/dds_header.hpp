#pragma once

#include <array>
#include <cstdint>

namespace photorealism {
namespace frame_capture {

constexpr std::size_t kDdsHeaderBytes = 4 + 124 + 20;

std::array<std::uint8_t, kDdsHeaderBytes> dds_header(
    unsigned width, unsigned height, unsigned dxgi_format);
unsigned bytes_per_pixel(unsigned dxgi_format);

}
}
