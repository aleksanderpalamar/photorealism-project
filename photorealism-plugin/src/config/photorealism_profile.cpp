#include "photorealism_profile.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace photorealism {
namespace {

constexpr const char kTonemapPrefix[] = "tonemap_";
constexpr unsigned kReferenceSet = 4;

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

bool apply_set_choice(PhotorealismProfile* profile, const char* key, const char* value) {
    if (std::strcmp(key, "tonemap_set") != 0) {
        return false;
    }
    const long chosen = std::strtol(value, nullptr, 10);
    if (chosen < 1 || chosen > static_cast<long>(kProfileTonemapSets)) {
        return false;
    }
    profile->tonemap_set = static_cast<unsigned>(chosen);
    return true;
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
    return apply_set_choice(profile, key, value) ||
           apply_scalar(profile, key, value) ||
           apply_tonemap(profile, key, value);
}

const PhotorealismTonemap& active_tonemap(const PhotorealismProfile& profile) {
    const unsigned index =
        profile.tonemap_set >= 1 && profile.tonemap_set <= kProfileTonemapSets
            ? profile.tonemap_set - 1
            : 0;
    return profile.sets[index];
}

bool uses_pending_controls(const PhotorealismTonemap& tonemap) {
    return tonemap.pre_exposure != 0.0f || tonemap.pre_contrast != 0.0f ||
           tonemap.dynamic_contrast != 1.0f || tonemap.night_exposure != 0.0f;
}

PhotorealismProfile reference_profile() {
    PhotorealismProfile profile;
    profile.tonemap_set = kReferenceSet;
    PhotorealismTonemap& set = profile.sets[kReferenceSet - 1];
    set.temperature = 6500.0f;
    set.exposure = -0.06f;
    set.saturation = 1.00f;
    set.contrast = 0.99f;
    set.vibrance = 0.00f;
    set.shadows = -0.01f;
    set.highlights = -0.07f;
    set.blacks = 0.00f;
    set.whites = -0.01f;
    set.night_exposure = 0.00f;
    profile.sharpness = 6.0f;
    profile.sharpen_edges = 4.0f;
    profile.ssao_intensity = 1.5f;
    return profile;
}

}
