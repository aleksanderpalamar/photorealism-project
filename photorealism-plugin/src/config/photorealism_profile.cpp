#include "photorealism_profile.hpp"

#include <cstdlib>
#include <cstring>

namespace photorealism {
namespace {

constexpr const char kTonemapPrefix[] = "tonemap_";

float number(const char* value) {
    return static_cast<float>(std::strtod(value, nullptr));
}

const TonemapField* field_named(const char* name, std::size_t length) {
    for (std::size_t index = 0; index < kTonemapFieldCount; ++index) {
        const TonemapField& field = kTonemapFields[index];
        const bool same = std::strlen(field.name) == length &&
                          std::strncmp(field.name, name, length) == 0;
        if (same) {
            return &field;
        }
    }
    return nullptr;
}

}

const TonemapField kTonemapFields[] = {
    {"temperature", &PhotorealismTonemap::temperature, 0},
    {"dynamic_contrast", &PhotorealismTonemap::dynamic_contrast, 2},
    {"pre_exposure", &PhotorealismTonemap::pre_exposure, 2},
    {"pre_contrast", &PhotorealismTonemap::pre_contrast, 2},
    {"exposure", &PhotorealismTonemap::exposure, 2},
    {"saturation", &PhotorealismTonemap::saturation, 2},
    {"contrast", &PhotorealismTonemap::contrast, 2},
    {"vibrance", &PhotorealismTonemap::vibrance, 2},
    {"shadows", &PhotorealismTonemap::shadows, 2},
    {"highlights", &PhotorealismTonemap::highlights, 2},
    {"blacks", &PhotorealismTonemap::blacks, 2},
    {"whites", &PhotorealismTonemap::whites, 2},
    {"night_exposure", &PhotorealismTonemap::night_exposure, 2},
};

const std::size_t kTonemapFieldCount =
    sizeof(kTonemapFields) / sizeof(kTonemapFields[0]);

bool apply_tonemap_key(
    PhotorealismTonemap* sets, const char* key, const char* value) {
    if (sets == nullptr || key == nullptr || value == nullptr) {
        return false;
    }
    const std::size_t prefix = sizeof(kTonemapPrefix) - 1;
    const std::size_t length = std::strlen(key);
    if (length < prefix + 3 || std::strncmp(key, kTonemapPrefix, prefix) != 0) {
        return false;
    }
    const char digit = key[length - 1];
    if (key[length - 2] != '_' || digit < '1' || digit > '5') {
        return false;
    }
    const TonemapField* field = field_named(key + prefix, length - prefix - 2);
    if (field == nullptr) {
        return false;
    }
    sets[digit - '1'].*(field->member) = number(value);
    return true;
}

unsigned active_set_index(float lighting_method) {
    const float rounded = lighting_method + 0.5f;
    if (!(rounded >= 1.0f)) {
        return 0;
    }
    if (rounded >= static_cast<float>(kLightingMethods)) {
        return kLightingMethods - 1;
    }
    return static_cast<unsigned>(rounded);
}

bool uses_pre_tone_controls(const PhotorealismTonemap& tonemap) {
    return tonemap.pre_exposure != 0.0f || tonemap.pre_contrast != 0.0f ||
           tonemap.dynamic_contrast != 1.0f;
}

}
