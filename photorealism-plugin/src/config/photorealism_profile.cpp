#include "photorealism_profile.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace photorealism {
namespace {

constexpr const char kTonemapPrefix[] = "tonemap_";

struct TonemapField {
    const char* name;
    float PhotorealismTonemap::*member;
};

constexpr TonemapField kTonemapFields[] = {
    {"temperature", &PhotorealismTonemap::temperature},
    {"pre_exposure", &PhotorealismTonemap::pre_exposure},
    {"pre_contrast", &PhotorealismTonemap::pre_contrast},
    {"dynamic_contrast", &PhotorealismTonemap::dynamic_contrast},
    {"exposure", &PhotorealismTonemap::exposure},
    {"saturation", &PhotorealismTonemap::saturation},
    {"contrast", &PhotorealismTonemap::contrast},
    {"vibrance", &PhotorealismTonemap::vibrance},
    {"shadows", &PhotorealismTonemap::shadows},
    {"highlights", &PhotorealismTonemap::highlights},
    {"blacks", &PhotorealismTonemap::blacks},
    {"whites", &PhotorealismTonemap::whites},
    {"night_exposure", &PhotorealismTonemap::night_exposure},
};

struct ScalarField {
    const char* name;
    float PhotorealismProfile::*member;
};

constexpr ScalarField kScalarFields[] = {
    {"sharpness", &PhotorealismProfile::sharpness},
    {"sharpen_edges", &PhotorealismProfile::sharpen_edges},
    {"ssao_intensity", &PhotorealismProfile::ssao_intensity},
};

float number(const char* value) {
    return static_cast<float>(std::strtod(value, nullptr));
}

bool apply_scalar(PhotorealismProfile* profile, const char* key, const char* value) {
    for (const ScalarField& field : kScalarFields) {
        if (std::strcmp(field.name, key) != 0) {
            continue;
        }
        profile->*(field.member) = number(value);
        return true;
    }
    return false;
}

bool apply_tonemap(PhotorealismProfile* profile, const char* key, const char* value) {
    const std::size_t prefix = sizeof(kTonemapPrefix) - 1;
    const std::size_t length = std::strlen(key);
    if (length < prefix + 3 || std::strncmp(key, kTonemapPrefix, prefix) != 0) {
        return false;
    }
    const char digit = key[length - 1];
    if (key[length - 2] != '_' || digit < '1' || digit > '5') {
        return false;
    }
    const std::size_t name_length = length - prefix - 2;
    PhotorealismTonemap& set = profile->sets[static_cast<unsigned>(digit - '1')];
    for (const TonemapField& field : kTonemapFields) {
        const bool same = std::strlen(field.name) == name_length &&
                          std::strncmp(field.name, key + prefix, name_length) == 0;
        if (!same) {
            continue;
        }
        set.*(field.member) = number(value);
        return true;
    }
    return false;
}

}

bool apply_profile_key(
    PhotorealismProfile* profile, const char* key, const char* value) {
    if (profile == nullptr || key == nullptr || value == nullptr) {
        return false;
    }
    return apply_scalar(profile, key, value) ||
           apply_tonemap(profile, key, value);
}

unsigned tonemap_set_number(float chosen) {
    const float rounded = chosen + 0.5f;
    if (!(rounded >= 1.0f)) {
        return 1;
    }
    if (rounded >= static_cast<float>(kProfileTonemapSets)) {
        return kProfileTonemapSets;
    }
    return static_cast<unsigned>(rounded);
}

const PhotorealismTonemap& active_tonemap(
    const PhotorealismProfile& profile, float chosen) {
    return profile.sets[tonemap_set_number(chosen) - 1];
}

bool uses_pending_controls(const PhotorealismTonemap& tonemap) {
    return tonemap.pre_exposure != 0.0f || tonemap.pre_contrast != 0.0f ||
           tonemap.dynamic_contrast != 1.0f;
}

}
