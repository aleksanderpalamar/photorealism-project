#pragma once

#include "../config/settings.hpp"

#include <cstddef>

namespace photorealism {
namespace overlay {

struct GradeKey {
    float Settings::*member;
    const char* key;
};

constexpr GradeKey kGradeKeys[] = {
    {&Settings::temperature, "temperature_delta"},
    {&Settings::exposure, "exposure_delta"},
    {&Settings::contrast, "contrast_delta"},
    {&Settings::saturation, "saturation_delta"},
    {&Settings::vibrance, "vibrance_delta"},
    {&Settings::shadows, "shadows_delta"},
    {&Settings::highlights, "highlights_delta"},
    {&Settings::blacks, "blacks_delta"},
    {&Settings::whites, "whites_delta"},
    {&Settings::local_contrast, "local_contrast_delta"},
    {&Settings::sharpness, "sharpness_delta"},
    {&Settings::vignette, "vignette_delta"},
    {&Settings::black_lift_r, "black_lift_r_delta"},
    {&Settings::black_lift_g, "black_lift_g_delta"},
    {&Settings::black_lift_b, "black_lift_b_delta"},
    {&Settings::highlight_rolloff, "highlight_rolloff_delta"},
    {&Settings::tint, "tint_delta"},
};

constexpr std::size_t kGradeKeyCount =
    sizeof(kGradeKeys) / sizeof(kGradeKeys[0]);

constexpr const char* kUserSection = "module.user.0.20.0";

const char* grade_key_for(float Settings::*member);

}
}
