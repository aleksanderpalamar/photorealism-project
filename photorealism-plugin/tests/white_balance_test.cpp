#include <cassert>
#include <cmath>

namespace {
constexpr float kLumaR = 0.2126f;
constexpr float kLumaG = 0.7152f;
constexpr float kLumaB = 0.0722f;

float clamp_value(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

struct Balance {
    float r;
    float g;
    float b;
};

Balance raw_balance(float temperature, float tint) {
    const float shift = clamp_value((temperature - 6500.0f) / 3500.0f, -1.0f, 1.0f);
    const float t = clamp_value(tint, -1.0f, 1.0f);
    return Balance{
        1.0f - 0.08f * shift - 0.05f * t,
        1.0f + 0.10f * t,
        1.0f + 0.10f * shift - 0.05f * t};
}

float luminance(const Balance& b) {
    return kLumaR * b.r + kLumaG * b.g + kLumaB * b.b;
}

Balance normalized_balance(float temperature, float tint) {
    const Balance b = raw_balance(temperature, tint);
    const float l = luminance(b);
    const float d = l > 1e-4f ? l : 1e-4f;
    return Balance{b.r / d, b.g / d, b.b / d};
}

bool near(float v, float target, float tol) {
    return std::fabs(v - target) <= tol;
}
}

int main() {
    {
        const Balance raw = raw_balance(6400.0f, 0.50f);
        assert(near(raw.r, 0.977286f, 1e-5f));
        assert(near(raw.g, 1.050000f, 1e-5f));
        assert(near(raw.b, 0.972143f, 1e-5f));

        assert(near(luminance(raw), 1.028920f, 1e-5f));
        assert(near(std::log2(luminance(raw)), 0.041130f, 1e-5f));

        const Balance norm = normalized_balance(6400.0f, 0.50f);
        assert(near(luminance(norm), 1.0f, 1e-6f));
    }

    {
        for (int ti = -20; ti <= 20; ++ti) {
            for (int tk = 3000; tk <= 9000; tk += 250) {
                const float tint = static_cast<float>(ti) / 20.0f;
                const float temperature = static_cast<float>(tk);
                assert(near(
                    luminance(normalized_balance(temperature, tint)),
                    1.0f, 1e-6f));
            }
        }
    }

    {
        for (int ti = -20; ti <= 20; ++ti) {
            const float tint = static_cast<float>(ti) / 20.0f;
            const Balance raw = raw_balance(6400.0f, tint);
            const Balance norm = normalized_balance(6400.0f, tint);
            assert(near(norm.r / norm.g, raw.r / raw.g, 1e-6f));
            assert(near(norm.b / norm.g, raw.b / raw.g, 1e-6f));
        }
    }

    {
        const float low = std::log2(luminance(raw_balance(6400.0f, 0.0f)));
        const float high = std::log2(luminance(raw_balance(6400.0f, 1.0f)));

        assert(high - low > 0.075f);
        assert(std::pow(2.0f, high - low) - 1.0f > 0.05f);

        const float nlow = std::log2(luminance(normalized_balance(6400.0f, 0.0f)));
        const float nhigh = std::log2(luminance(normalized_balance(6400.0f, 1.0f)));
        assert(near(nhigh - nlow, 0.0f, 1e-6f));
    }

    {
        float smallest = 1e9f;
        for (int ti = -20; ti <= 20; ++ti) {
            for (int tk = 3000; tk <= 9000; tk += 250) {
                const float l = luminance(raw_balance(
                    static_cast<float>(tk), static_cast<float>(ti) / 20.0f));
                if (l < smallest) {
                    smallest = l;
                }
            }
        }
        assert(smallest > 0.9f);
    }

    return 0;
}
