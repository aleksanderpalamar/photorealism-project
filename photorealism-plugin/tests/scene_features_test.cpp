#include "../src/scene_features.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <vector>

using photorealism::compute_scene_features;
using photorealism::SceneFeatures;

namespace {
std::vector<unsigned char> make_image(
    unsigned width,
    unsigned height,
    unsigned pitch,
    bool bgra,
    const std::vector<unsigned char>& rgb_rows) {
    std::vector<unsigned char> buffer(
        static_cast<std::size_t>(pitch) * height, 0u);
    for (unsigned y = 0u; y < height; ++y) {
        for (unsigned x = 0u; x < width; ++x) {
            const std::size_t source = (static_cast<std::size_t>(y) * width + x) * 3u;
            const unsigned char r = rgb_rows[source];
            const unsigned char g = rgb_rows[source + 1u];
            const unsigned char b = rgb_rows[source + 2u];
            unsigned char* texel =
                buffer.data() + static_cast<std::size_t>(y) * pitch + x * 4u;
            texel[0] = bgra ? b : r;
            texel[1] = g;
            texel[2] = bgra ? r : b;
            texel[3] = 255u;
        }
    }
    return buffer;
}

bool near(float value, float target, float tolerance) {
    return std::fabs(value - target) <= tolerance;
}
}

int main() {
    {
        std::vector<unsigned char> rgb(4u * 4u * 3u, 128u);
        const std::vector<unsigned char> image =
            make_image(4u, 4u, 64u, true, rgb);
        const SceneFeatures f =
            compute_scene_features(image.data(), 4u, 4u, 64u, true);
        assert(f.valid);
        assert(f.sample_pixels == 16u);
        assert(near(f.median, 128.0f, 0.01f));
        assert(near(f.dynamic_range, 0.0f, 0.01f));
        assert(near(f.saturation, 0.0f, 0.001f));
        assert(near(f.sky_r_over_b, 1.0f, 0.001f));
    }

    {
        std::vector<unsigned char> rgb;
        for (unsigned y = 0u; y < 4u; ++y) {
            for (unsigned x = 0u; x < 4u; ++x) {
                const bool top = y < 2u;
                rgb.push_back(top ? 200u : 10u);
                rgb.push_back(top ? 100u : 10u);
                rgb.push_back(top ? 50u : 10u);
            }
        }
        const std::vector<unsigned char> bgra_image =
            make_image(4u, 4u, 32u, true, rgb);
        const SceneFeatures bgra =
            compute_scene_features(bgra_image.data(), 4u, 4u, 32u, true);
        const std::vector<unsigned char> rgba_image =
            make_image(4u, 4u, 32u, false, rgb);
        const SceneFeatures rgba =
            compute_scene_features(rgba_image.data(), 4u, 4u, 32u, false);

        assert(near(bgra.sky_r_over_b, 4.0f, 0.001f));
        assert(near(rgba.sky_r_over_b, 4.0f, 0.001f));
        assert(bgra.sky_r_over_b > 1.0f);
    }

    {
        std::vector<unsigned char> rgb(4u * 4u * 3u, 0u);
        const std::vector<unsigned char> image =
            make_image(4u, 4u, 32u, true, rgb);
        const SceneFeatures f =
            compute_scene_features(image.data(), 4u, 4u, 32u, true);
        assert(f.valid);
        assert(near(f.sky_r_over_b, 1.0f, 0.001f));
    }

    {
        const SceneFeatures none =
            compute_scene_features(nullptr, 4u, 4u, 32u, true);
        assert(!none.valid);
        std::vector<unsigned char> rgb(4u * 3u, 128u);
        const std::vector<unsigned char> image =
            make_image(4u, 1u, 32u, true, rgb);
        const SceneFeatures empty =
            compute_scene_features(image.data(), 0u, 0u, 32u, true);
        assert(!empty.valid);
    }

    {
        struct Reference {
            const char* name;
            unsigned condition;
            float sky_r_over_b;
            float median;
            float dynamic_range;
            float saturation;
        };
        const Reference refs[] = {
            {"23-47-51 anoitecer", 0u, 0.915f, 13.1f, 55.0f, 0.421f},
            {"23-33-14 encoberto", 1u, 0.967f, 21.2f, 131.2f, 0.338f},
            {"11-12-25 sol baixo com neblina", 2u, 1.005f, 21.0f, 102.4f, 0.260f},
            {"11-12-15 sol baixo com neblina", 2u, 1.000f, 30.3f, 95.2f, 0.212f},
            {"15-56-22 dia claro", 3u, 0.938f, 43.9f, 157.7f, 0.183f},
        };
        const std::size_t count = sizeof(refs) / sizeof(refs[0]);

        float values[count][4];
        for (std::size_t i = 0u; i < count; ++i) {
            values[i][0] = refs[i].sky_r_over_b;
            values[i][1] = refs[i].median;
            values[i][2] = refs[i].dynamic_range;
            values[i][3] = refs[i].saturation;
        }

        for (unsigned f = 0u; f < 4u; ++f) {
            double sum = 0.0;
            for (std::size_t i = 0u; i < count; ++i) {
                sum += values[i][f];
            }
            const double mean = sum / count;
            double variance = 0.0;
            for (std::size_t i = 0u; i < count; ++i) {
                const double delta = values[i][f] - mean;
                variance += delta * delta;
            }
            const double deviation = std::sqrt(variance / count);
            assert(deviation > 0.0);
            for (std::size_t i = 0u; i < count; ++i) {
                values[i][f] =
                    static_cast<float>((values[i][f] - mean) / deviation);
            }
        }

        float smallest_between = 1e9f;
        float largest_within = 0.0f;
        for (std::size_t i = 0u; i < count; ++i) {
            for (std::size_t j = i + 1u; j < count; ++j) {
                double squared = 0.0;
                for (unsigned f = 0u; f < 4u; ++f) {
                    const double delta = values[i][f] - values[j][f];
                    squared += delta * delta;
                }
                const float distance = static_cast<float>(std::sqrt(squared));
                if (refs[i].condition == refs[j].condition) {
                    if (distance > largest_within) {
                        largest_within = distance;
                    }
                } else if (distance < smallest_between) {
                    smallest_between = distance;
                }
            }
        }

        assert(largest_within < 1.30f);
        assert(smallest_between > 1.50f);

        assert(smallest_between > largest_within);
        assert(smallest_between / largest_within < 2.0f);
    }

    return 0;
}
