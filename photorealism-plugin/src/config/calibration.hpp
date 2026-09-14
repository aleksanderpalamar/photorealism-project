#pragma once

#include "photorealism_profile.hpp"
#include "settings.hpp"

namespace photorealism {

struct CalibrationLayer {
    bool enabled;
    float temperature;
    float exposure;
    float contrast;
    float saturation;
    float vibrance;
    float shadows;
    float highlights;
    float blacks;
    float whites;
    float local_contrast;
    float sharpness;
    float vignette;

    float black_lift_r;
    float black_lift_g;
    float black_lift_b;
    float highlight_rolloff;
    float tint;
};

struct CalibrationStack {
    CalibrationLayer base;
    CalibrationLayer visual_0_2;
    CalibrationLayer rain_overcast_0_3;
    CalibrationLayer user_0_20;
    PhotorealismProfile profile;
    Settings modules;
};
}
