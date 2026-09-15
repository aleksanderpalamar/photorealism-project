#pragma once

#include <cstddef>

namespace photorealism {

constexpr unsigned kProfileTonemapSets = 5;
constexpr unsigned kLightingMethods = 4;
constexpr float kReferenceLightingMethod = 3.0f;

struct PhotorealismTonemap {
    float temperature = 6500.0f;
    float pre_exposure = 0.0f;
    float pre_contrast = 0.0f;
    float dynamic_contrast = 1.0f;
    float exposure = 0.0f;
    float saturation = 1.0f;
    float contrast = 1.0f;
    float vibrance = 0.0f;
    float shadows = 0.0f;
    float highlights = 0.0f;
    float blacks = 0.0f;
    float whites = 0.0f;
    float night_exposure = 0.0f;
};

struct TonemapField {
    const char* name;
    float PhotorealismTonemap::*member;
    int decimals;
};

extern const TonemapField kTonemapFields[];
extern const std::size_t kTonemapFieldCount;

bool apply_tonemap_key(
    PhotorealismTonemap* sets, const char* key, const char* value);
unsigned active_set_index(float lighting_method);
bool uses_pre_tone_controls(const PhotorealismTonemap& tonemap);
void reference_tonemap_sets(PhotorealismTonemap* sets);

}
