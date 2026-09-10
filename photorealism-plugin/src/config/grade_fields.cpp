#include "grade_fields.hpp"

#include "text_utils.hpp"

#include <windows.h>

#include <cstddef>
#include <cstring>

namespace photorealism {
namespace {

struct GradeField {
    const char* key;
    float Settings::*effective;
    float CalibrationLayer::*layer;
};

constexpr GradeField kGradeFields[] = {
    {"temperature", &Settings::temperature, &CalibrationLayer::temperature},
    {"exposure", &Settings::exposure, &CalibrationLayer::exposure},
    {"contrast", &Settings::contrast, &CalibrationLayer::contrast},
    {"saturation", &Settings::saturation, &CalibrationLayer::saturation},
    {"vibrance", &Settings::vibrance, &CalibrationLayer::vibrance},
    {"shadows", &Settings::shadows, &CalibrationLayer::shadows},
    {"highlights", &Settings::highlights, &CalibrationLayer::highlights},
    {"blacks", &Settings::blacks, &CalibrationLayer::blacks},
    {"whites", &Settings::whites, &CalibrationLayer::whites},
    {"local_contrast", &Settings::local_contrast,
     &CalibrationLayer::local_contrast},
    {"sharpness", &Settings::sharpness, &CalibrationLayer::sharpness},
    {"vignette", &Settings::vignette, &CalibrationLayer::vignette},
    {"black_lift_r", &Settings::black_lift_r, &CalibrationLayer::black_lift_r},
    {"black_lift_g", &Settings::black_lift_g, &CalibrationLayer::black_lift_g},
    {"black_lift_b", &Settings::black_lift_b, &CalibrationLayer::black_lift_b},
    {"highlight_rolloff", &Settings::highlight_rolloff,
     &CalibrationLayer::highlight_rolloff},
    {"tint", &Settings::tint, &CalibrationLayer::tint},
};

constexpr std::size_t kGradeFieldCount =
    sizeof(kGradeFields) / sizeof(kGradeFields[0]);

const char* strip_delta_suffix(const char* key, char* buffer, std::size_t size) {
    static constexpr char kSuffix[] = "_delta";
    const std::size_t suffix_length = sizeof(kSuffix) - 1;
    const std::size_t key_length = std::strlen(key);
    const bool has_suffix =
        key_length > suffix_length &&
        _stricmp(key + key_length - suffix_length, kSuffix) == 0;
    const std::size_t copy_length = key_length - suffix_length;
    if (!has_suffix || copy_length >= size) {
        return key;
    }
    std::memcpy(buffer, key, copy_length);
    buffer[copy_length] = '\0';
    return buffer;
}

}

void apply_layer_setting(
    CalibrationLayer* layer, const char* raw_key, const char* value) {
    if (_stricmp(raw_key, "enabled") == 0) {
        layer->enabled = config_text::parse_bool(value);
        return;
    }

    char buffer[64] = {};
    const char* key = strip_delta_suffix(raw_key, buffer, sizeof(buffer));

    if (_stricmp(key, "black_lift") == 0) {
        const float number = config_text::to_number(value);
        layer->black_lift_r = number;
        layer->black_lift_g = number;
        layer->black_lift_b = number;
        return;
    }

    for (std::size_t i = 0; i < kGradeFieldCount; ++i) {
        if (_stricmp(kGradeFields[i].key, key) != 0) {
            continue;
        }
        layer->*(kGradeFields[i].layer) = config_text::to_number(value);
        return;
    }
}
void copy_base_layer(Settings* settings, const CalibrationLayer& layer) {
    for (const GradeField& field : kGradeFields) {
        settings->*(field.effective) = layer.*(field.layer);
    }
}

void add_delta_layer(Settings* settings, const CalibrationLayer& layer) {
    if (!layer.enabled) {
        return;
    }
    for (const GradeField& field : kGradeFields) {
        settings->*(field.effective) += layer.*(field.layer);
    }
}

}
