#pragma once

namespace photorealism {

constexpr unsigned kProfileTonemapSets = 5;

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

struct PhotorealismProfile {
    unsigned tonemap_set = 1;
    PhotorealismTonemap sets[kProfileTonemapSets];
    float sharpness = 0.0f;
    float sharpen_edges = 0.0f;
    float ssao_intensity = 1.0f;
};

bool apply_profile_key(
    PhotorealismProfile* profile, const char* key, const char* value);
const PhotorealismTonemap& active_tonemap(const PhotorealismProfile& profile);
bool uses_pending_controls(const PhotorealismTonemap& tonemap);
PhotorealismProfile reference_profile();

}
