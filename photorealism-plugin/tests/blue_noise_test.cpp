#include "../src/fsr/blue_noise.cpp"
#include "../src/fsr/rcas_constants.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace photorealism::fsr;

namespace {

double box_mean_variance(const std::vector<float>& values, unsigned size, unsigned box) {
    double sum = 0.0;
    double squares = 0.0;
    unsigned count = 0;
    for (unsigned y = 0; y < size; ++y) {
        for (unsigned x = 0; x < size; ++x) {
            double mean = 0.0;
            for (unsigned by = 0; by < box; ++by) {
                for (unsigned bx = 0; bx < box; ++bx) {
                    mean += values[((y + by) % size) * size + (x + bx) % size];
                }
            }
            mean /= box * box;
            sum += mean;
            squares += mean * mean;
            ++count;
        }
    }
    const double average = sum / count;
    return squares / count - average * average;
}

std::vector<float> white_noise(unsigned size) {
    std::vector<float> values(size * size);
    std::uint32_t state = 12345u;
    for (float& value : values) {
        state = state * 1664525u + 1013904223u;
        value = static_cast<float>(state >> 8) / 16777216.0f;
    }
    return values;
}

void every_rank_appears_exactly_once() {
    std::vector<float> values = generate_blue_noise(kGrainTileSize);
    assert(values.size() == kGrainTileSize * kGrainTileSize);
    std::sort(values.begin(), values.end());
    const float total = static_cast<float>(values.size());
    for (unsigned index = 0; index < values.size(); ++index) {
        assert(values[index] == (static_cast<float>(index) + 0.5f) / total);
    }
}

void the_generation_is_deterministic() {
    assert(generate_blue_noise(16) == generate_blue_noise(16));
}

void low_frequencies_are_missing_compared_to_white_noise() {
    const std::vector<float> blue = generate_blue_noise(kGrainTileSize);
    const std::vector<float> white = white_noise(kGrainTileSize);
    const unsigned boxes[] = {2u, 4u, 8u};
    const double ceilings[] = {0.6, 0.3, 0.2};
    for (unsigned index = 0; index < 3; ++index) {
        const unsigned box = boxes[index];
        const double blue_variance = box_mean_variance(blue, kGrainTileSize, box);
        const double white_variance = box_mean_variance(white, kGrainTileSize, box);
        assert(blue_variance < ceilings[index] * white_variance);
    }
}

void the_temporal_sequence_has_no_bias() {
    const std::vector<float> blue = generate_blue_noise(kGrainTileSize);
    for (unsigned index = 0; index < blue.size(); index += 97) {
        double sum = 0.0;
        const unsigned frames = 1000;
        for (unsigned frame = 0; frame < frames; ++frame) {
            const double shifted = blue[index] + grain_phase(frame);
            sum += (shifted - std::floor(shifted)) - 0.5;
        }
        assert(std::fabs(sum / frames) < 0.002);
    }
}

}

int main() {
    every_rank_appears_exactly_once();
    the_generation_is_deterministic();
    low_frequencies_are_missing_compared_to_white_noise();
    the_temporal_sequence_has_no_bias();
    std::printf("blue_noise_test ok\n");
    return 0;
}
