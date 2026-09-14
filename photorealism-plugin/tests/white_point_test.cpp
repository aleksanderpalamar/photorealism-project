#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

float smoothstep_value(float edge0, float edge1, float x) {
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float apply_white_point(float color, float whites, float luma) {
    const float mask = smoothstep_value(0.30f, 1.0f, luma);
    const float white_point = std::clamp(1.0f - 0.5f * whites, 0.25f, 4.0f);
    return color / (1.0f + (white_point - 1.0f) * mask);
}

float encode(float linear) {
    return linear <= 0.0031308f ? linear * 12.92f
                                : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}

float decode(float code) {
    const float value = code / 255.0f;
    return value <= 0.04045f ? value / 12.92f
                             : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

float output_code(float input_code, float whites) {
    const float linear = decode(input_code);
    return std::min(encode(apply_white_point(linear, whites, linear)), 1.0f) * 255.0f;
}

}

int main() {
    assert(std::fabs(output_code(230.0f, 0.0f) - 230.0f) < 0.5f);
    assert(output_code(230.0f, -0.13f) < 230.0f - 3.0f);
    assert(output_code(230.0f, 0.13f) > 230.0f + 3.0f);
    assert(std::fabs(output_code(40.0f, -0.13f) - 40.0f) < 0.5f);
    float previous = 0.0f;
    for (float whites = -1.0f; whites <= 1.0f; whites += 0.25f) {
        const float code = output_code(200.0f, whites);
        assert(code >= previous);
        previous = code;
    }
    std::printf("white_point_test ok\n");
    return 0;
}
