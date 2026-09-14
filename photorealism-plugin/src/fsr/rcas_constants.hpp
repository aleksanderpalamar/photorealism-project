#pragma once

#include <cmath>

namespace photorealism {
namespace fsr {

constexpr unsigned kGrainTileSize = 64;
constexpr double kGoldenRatioConjugate = 0.6180339887498949;

struct RcasConstants {
    float rcas_con;
    float decode_before_write;
    float grain_amount;
    float grain_phase;
    float output_size[2];
    float grain_tile_size;
    float padding;
};

inline float rcas_stops_from_sharpness(float sharpness) {
    return (-2.0f * sharpness) + 2.0f;
}

inline float rcas_con_from_stops(float stops) {
    return std::exp2(-stops);
}

inline float grain_phase(unsigned frame) {
    const double position = static_cast<double>(frame) * kGoldenRatioConjugate;
    return static_cast<float>(position - std::floor(position));
}

}
}
