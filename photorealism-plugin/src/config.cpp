#include "config.hpp"

#include "runtime.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace photorealism {
namespace {

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
};

struct CalibrationStack {
    bool enabled;
    CalibrationLayer base;
    CalibrationLayer visual_0_2;
    CalibrationLayer rain_overcast_0_3;
    float depth_near_plane;
    float depth_preview_distance;
    float depth_vertical_fov;
    bool ssao_enabled;
    float ssao_radius;
    float ssao_intensity;
    float ssao_bias;
    float ssao_fade_start;
    float ssao_fade_end;
    float ssao_edge_rejection;
    bool ssao_refinement_enabled;
    float ssao_highlight_start;
    float ssao_highlight_end;
    float ssao_highlight_ao_floor;
    bool ssao_interior_enabled;
    float ssao_interior_near_start;
    float ssao_interior_near_end;
    float ssao_interior_radius;
    float ssao_interior_intensity;
    float ssao_interior_bias;
    float ssao_interior_edge_rejection;
    bool temporal_enabled;
    float temporal_history_weight;
    float temporal_depth_rejection;
    float temporal_color_rejection;
};

enum class Section {
    plugin,
    base,
    visual_0_2,
    rain_overcast_0_3,
    depth_0_6_4,
    ssao_0_7_0,
    ssao_refinement_0_8_0,
    ssao_interior_0_9_0,
    temporal_0_10_0,
    unknown,
};

float clamp_value(float value, float minimum, float maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

char* trim(char* text) {
    while (*text != '\0' && std::isspace(static_cast<unsigned char>(*text))) {
        ++text;
    }

    char* end = text + std::strlen(text);
    while (end > text && std::isspace(static_cast<unsigned char>(end[-1]))) {
        --end;
    }
    *end = '\0';
    return text;
}

bool parse_bool(const char* value) {
    return _stricmp(value, "true") == 0 ||
           _stricmp(value, "yes") == 0 ||
           std::atoi(value) != 0;
}

Section parse_section(const char* name) {
    if (_stricmp(name, "plugin") == 0) {
        return Section::plugin;
    }
    if (_stricmp(name, "base.0.1.2") == 0) {
        return Section::base;
    }
    if (_stricmp(name, "module.visual.0.2.0") == 0) {
        return Section::visual_0_2;
    }
    if (_stricmp(name, "module.rain_overcast.0.3.0") == 0) {
        return Section::rain_overcast_0_3;
    }
    if (_stricmp(name, "depth.0.6.4") == 0) {
        return Section::depth_0_6_4;
    }
    if (_stricmp(name, "module.ssao.0.7.0") == 0) {
        return Section::ssao_0_7_0;
    }
    if (_stricmp(name, "module.ssao_refinement.0.8.0") == 0) {
        return Section::ssao_refinement_0_8_0;
    }
    if (_stricmp(name, "module.ssao_interior.0.9.0") == 0) {
        return Section::ssao_interior_0_9_0;
    }
    if (_stricmp(name, "module.temporal.0.10.0") == 0) {
        return Section::temporal_0_10_0;
    }
    return Section::unknown;
}

CalibrationLayer* layer_for_section(
    CalibrationStack* stack, Section section) {
    switch (section) {
        case Section::base:
            return &stack->base;
        case Section::visual_0_2:
            return &stack->visual_0_2;
        case Section::rain_overcast_0_3:
            return &stack->rain_overcast_0_3;
        default:
            return nullptr;
    }
}

const char* normalized_layer_key(const char* key) {
    static constexpr const char suffix[] = "_delta";
    const size_t key_length = std::strlen(key);
    const size_t suffix_length = sizeof(suffix) - 1;
    if (key_length > suffix_length &&
        _stricmp(key + key_length - suffix_length, suffix) == 0) {
        static thread_local char normalized[64] = {};
        const size_t copy_length = key_length - suffix_length;
        if (copy_length >= sizeof(normalized)) {
            return key;
        }
        std::memcpy(normalized, key, copy_length);
        normalized[copy_length] = '\0';
        return normalized;
    }
    return key;
}

void assign_layer_value(
    CalibrationLayer* layer, const char* raw_key, const char* value) {
    if (layer == nullptr) {
        return;
    }
    if (_stricmp(raw_key, "enabled") == 0) {
        layer->enabled = parse_bool(value);
        return;
    }

    const char* key = normalized_layer_key(raw_key);
    const float number = static_cast<float>(std::strtod(value, nullptr));
    if (_stricmp(key, "temperature") == 0) {
        layer->temperature = number;
    } else if (_stricmp(key, "exposure") == 0) {
        layer->exposure = number;
    } else if (_stricmp(key, "contrast") == 0) {
        layer->contrast = number;
    } else if (_stricmp(key, "saturation") == 0) {
        layer->saturation = number;
    } else if (_stricmp(key, "vibrance") == 0) {
        layer->vibrance = number;
    } else if (_stricmp(key, "shadows") == 0) {
        layer->shadows = number;
    } else if (_stricmp(key, "highlights") == 0) {
        layer->highlights = number;
    } else if (_stricmp(key, "blacks") == 0) {
        layer->blacks = number;
    } else if (_stricmp(key, "whites") == 0) {
        layer->whites = number;
    } else if (_stricmp(key, "local_contrast") == 0) {
        layer->local_contrast = number;
    } else if (_stricmp(key, "sharpness") == 0) {
        layer->sharpness = number;
    } else if (_stricmp(key, "vignette") == 0) {
        layer->vignette = number;
    }
}

void assign_depth_value(
    CalibrationStack* stack, const char* key, const char* value) {
    if (stack == nullptr || key == nullptr || value == nullptr) {
        return;
    }
    const float number = static_cast<float>(std::strtod(value, nullptr));
    if (_stricmp(key, "near_plane") == 0) {
        stack->depth_near_plane = number;
    } else if (_stricmp(key, "preview_distance") == 0) {
        stack->depth_preview_distance = number;
    } else if (_stricmp(key, "vertical_fov") == 0) {
        stack->depth_vertical_fov = number;
    }
}

void assign_ssao_value(
    CalibrationStack* stack, const char* key, const char* value) {
    if (stack == nullptr || key == nullptr || value == nullptr) {
        return;
    }
    if (_stricmp(key, "enabled") == 0) {
        stack->ssao_enabled = parse_bool(value);
        return;
    }

    const float number = static_cast<float>(std::strtod(value, nullptr));
    if (_stricmp(key, "radius") == 0) {
        stack->ssao_radius = number;
    } else if (_stricmp(key, "intensity") == 0) {
        stack->ssao_intensity = number;
    } else if (_stricmp(key, "bias") == 0) {
        stack->ssao_bias = number;
    } else if (_stricmp(key, "fade_start") == 0) {
        stack->ssao_fade_start = number;
    } else if (_stricmp(key, "fade_end") == 0) {
        stack->ssao_fade_end = number;
    } else if (_stricmp(key, "edge_rejection") == 0) {
        stack->ssao_edge_rejection = number;
    }
}

void assign_ssao_refinement_value(
    CalibrationStack* stack, const char* key, const char* value) {
    if (stack == nullptr || key == nullptr || value == nullptr) {
        return;
    }
    if (_stricmp(key, "enabled") == 0) {
        stack->ssao_refinement_enabled = parse_bool(value);
        return;
    }

    const float number = static_cast<float>(std::strtod(value, nullptr));
    if (_stricmp(key, "highlight_start") == 0) {
        stack->ssao_highlight_start = number;
    } else if (_stricmp(key, "highlight_end") == 0) {
        stack->ssao_highlight_end = number;
    } else if (_stricmp(key, "highlight_ao_floor") == 0) {
        stack->ssao_highlight_ao_floor = number;
    }
}

void assign_ssao_interior_value(
    CalibrationStack* stack, const char* key, const char* value) {
    if (stack == nullptr || key == nullptr || value == nullptr) {
        return;
    }
    if (_stricmp(key, "enabled") == 0) {
        stack->ssao_interior_enabled = parse_bool(value);
        return;
    }

    const float number = static_cast<float>(std::strtod(value, nullptr));
    if (_stricmp(key, "near_start") == 0) {
        stack->ssao_interior_near_start = number;
    } else if (_stricmp(key, "near_end") == 0) {
        stack->ssao_interior_near_end = number;
    } else if (_stricmp(key, "radius") == 0) {
        stack->ssao_interior_radius = number;
    } else if (_stricmp(key, "intensity") == 0) {
        stack->ssao_interior_intensity = number;
    } else if (_stricmp(key, "bias") == 0) {
        stack->ssao_interior_bias = number;
    } else if (_stricmp(key, "edge_rejection") == 0) {
        stack->ssao_interior_edge_rejection = number;
    }
}

void assign_temporal_value(
    CalibrationStack* stack, const char* key, const char* value) {
    if (stack == nullptr || key == nullptr || value == nullptr) {
        return;
    }
    if (_stricmp(key, "enabled") == 0) {
        stack->temporal_enabled = parse_bool(value);
        return;
    }

    const float number = static_cast<float>(std::strtod(value, nullptr));
    if (_stricmp(key, "history_weight") == 0) {
        stack->temporal_history_weight = number;
    } else if (_stricmp(key, "depth_rejection") == 0) {
        stack->temporal_depth_rejection = number;
    } else if (_stricmp(key, "color_rejection") == 0) {
        stack->temporal_color_rejection = number;
    }
}

CalibrationLayer reference_base() {
    CalibrationLayer layer = {};
    layer.enabled = true;
    layer.temperature = 6500.0f;
    layer.exposure = -0.09f;
    layer.contrast = 0.98f;
    layer.saturation = 0.95f;
    layer.vibrance = -0.05f;
    layer.shadows = 0.04f;
    layer.highlights = -0.05f;
    layer.blacks = -0.01f;
    layer.whites = 0.03f;
    layer.local_contrast = 0.12f;
    layer.sharpness = 0.18f;
    layer.vignette = 0.04f;
    return layer;
}

CalibrationLayer visual_delta_0_2() {
    CalibrationLayer layer = {};
    layer.enabled = true;
    layer.temperature = -100.0f;
    layer.exposure = 0.05f;
    layer.contrast = 0.08f;
    layer.saturation = 0.03f;
    layer.vibrance = 0.09f;
    layer.shadows = 0.04f;
    layer.highlights = -0.09f;
    layer.blacks = -0.04f;
    layer.whites = 0.01f;
    layer.local_contrast = 0.06f;
    layer.sharpness = 0.04f;
    layer.vignette = -0.005f;
    return layer;
}

CalibrationLayer rain_overcast_delta_0_3() {
    CalibrationLayer layer = {};
    layer.enabled = true;
    layer.temperature = 0.0f;
    layer.exposure = 0.01f;
    layer.contrast = 0.01f;
    layer.saturation = -0.01f;
    layer.vibrance = 0.01f;
    layer.shadows = 0.02f;
    layer.highlights = -0.04f;
    layer.blacks = -0.01f;
    layer.whites = 0.04f;
    layer.local_contrast = 0.06f;
    layer.sharpness = -0.02f;
    layer.vignette = -0.005f;
    return layer;
}

CalibrationStack reference_stack() {
    CalibrationStack stack = {};
    stack.enabled = true;
    stack.base = reference_base();
    stack.visual_0_2 = visual_delta_0_2();
    stack.rain_overcast_0_3 = rain_overcast_delta_0_3();
    stack.depth_near_plane = 0.1f;
    stack.depth_preview_distance = 50.0f;
    stack.depth_vertical_fov = 60.0f;
    stack.ssao_enabled = true;
    stack.ssao_radius = 0.8f;
    stack.ssao_intensity = 0.28f;
    stack.ssao_bias = 0.04f;
    stack.ssao_fade_start = 30.0f;
    stack.ssao_fade_end = 70.0f;
    stack.ssao_edge_rejection = 1.5f;
    stack.ssao_refinement_enabled = true;
    stack.ssao_highlight_start = 0.55f;
    stack.ssao_highlight_end = 0.95f;
    stack.ssao_highlight_ao_floor = 0.35f;
    stack.ssao_interior_enabled = true;
    stack.ssao_interior_near_start = 2.0f;
    stack.ssao_interior_near_end = 8.0f;
    stack.ssao_interior_radius = 0.45f;
    stack.ssao_interior_intensity = 0.20f;
    stack.ssao_interior_bias = 0.05f;
    stack.ssao_interior_edge_rejection = 1.75f;
    stack.temporal_enabled = true;
    stack.temporal_history_weight = 0.65f;
    stack.temporal_depth_rejection = 0.02f;
    stack.temporal_color_rejection = 0.08f;
    return stack;
}

void add_layer(Settings* settings, const CalibrationLayer& layer) {
    if (!layer.enabled) {
        return;
    }
    settings->temperature += layer.temperature;
    settings->exposure += layer.exposure;
    settings->contrast += layer.contrast;
    settings->saturation += layer.saturation;
    settings->vibrance += layer.vibrance;
    settings->shadows += layer.shadows;
    settings->highlights += layer.highlights;
    settings->blacks += layer.blacks;
    settings->whites += layer.whites;
    settings->local_contrast += layer.local_contrast;
    settings->sharpness += layer.sharpness;
    settings->vignette += layer.vignette;
}

Settings compose_stack(const CalibrationStack& stack) {
    Settings settings = {};
    settings.enabled = stack.enabled;
    if (stack.base.enabled) {
        settings.temperature = stack.base.temperature;
        settings.exposure = stack.base.exposure;
        settings.contrast = stack.base.contrast;
        settings.saturation = stack.base.saturation;
        settings.vibrance = stack.base.vibrance;
        settings.shadows = stack.base.shadows;
        settings.highlights = stack.base.highlights;
        settings.blacks = stack.base.blacks;
        settings.whites = stack.base.whites;
        settings.local_contrast = stack.base.local_contrast;
        settings.sharpness = stack.base.sharpness;
        settings.vignette = stack.base.vignette;
    }
    add_layer(&settings, stack.visual_0_2);
    add_layer(&settings, stack.rain_overcast_0_3);
    settings.depth_near_plane = stack.depth_near_plane;
    settings.depth_preview_distance = stack.depth_preview_distance;
    settings.depth_vertical_fov = stack.depth_vertical_fov;
    settings.ssao_enabled = stack.ssao_enabled;
    settings.ssao_radius = stack.ssao_radius;
    settings.ssao_intensity = stack.ssao_intensity;
    settings.ssao_bias = stack.ssao_bias;
    settings.ssao_fade_start = stack.ssao_fade_start;
    settings.ssao_fade_end = stack.ssao_fade_end;
    settings.ssao_edge_rejection = stack.ssao_edge_rejection;
    settings.ssao_refinement_enabled = stack.ssao_refinement_enabled;
    settings.ssao_highlight_start = stack.ssao_highlight_start;
    settings.ssao_highlight_end = stack.ssao_highlight_end;
    settings.ssao_highlight_ao_floor = stack.ssao_highlight_ao_floor;
    settings.ssao_interior_enabled = stack.ssao_interior_enabled;
    settings.ssao_interior_near_start = stack.ssao_interior_near_start;
    settings.ssao_interior_near_end = stack.ssao_interior_near_end;
    settings.ssao_interior_radius = stack.ssao_interior_radius;
    settings.ssao_interior_intensity = stack.ssao_interior_intensity;
    settings.ssao_interior_bias = stack.ssao_interior_bias;
    settings.ssao_interior_edge_rejection =
        stack.ssao_interior_edge_rejection;
    settings.temporal_enabled = stack.temporal_enabled;
    settings.temporal_history_weight = stack.temporal_history_weight;
    settings.temporal_depth_rejection = stack.temporal_depth_rejection;
    settings.temporal_color_rejection = stack.temporal_color_rejection;

    settings.temperature = clamp_value(settings.temperature, 3000.0f, 9000.0f);
    settings.exposure = clamp_value(settings.exposure, -2.0f, 2.0f);
    settings.contrast = clamp_value(settings.contrast, 0.5f, 1.5f);
    settings.saturation = clamp_value(settings.saturation, 0.0f, 2.0f);
    settings.vibrance = clamp_value(settings.vibrance, -1.0f, 1.0f);
    settings.shadows = clamp_value(settings.shadows, -1.0f, 1.0f);
    settings.highlights = clamp_value(settings.highlights, -1.0f, 1.0f);
    settings.blacks = clamp_value(settings.blacks, -1.0f, 1.0f);
    settings.whites = clamp_value(settings.whites, -1.0f, 1.0f);
    settings.local_contrast = clamp_value(settings.local_contrast, 0.0f, 1.0f);
    settings.sharpness = clamp_value(settings.sharpness, 0.0f, 1.0f);
    settings.vignette = clamp_value(settings.vignette, 0.0f, 0.5f);
    settings.depth_near_plane =
        clamp_value(settings.depth_near_plane, 0.001f, 10.0f);
    settings.depth_preview_distance =
        clamp_value(settings.depth_preview_distance, 1.0f, 10000.0f);
    settings.depth_vertical_fov =
        clamp_value(settings.depth_vertical_fov, 20.0f, 140.0f);
    settings.ssao_radius = clamp_value(settings.ssao_radius, 0.05f, 5.0f);
    settings.ssao_intensity =
        clamp_value(settings.ssao_intensity, 0.0f, 1.0f);
    settings.ssao_bias = clamp_value(settings.ssao_bias, 0.0f, 0.5f);
    settings.ssao_fade_start =
        clamp_value(settings.ssao_fade_start, 1.0f, 500.0f);
    settings.ssao_fade_end =
        clamp_value(settings.ssao_fade_end, 2.0f, 1000.0f);
    if (settings.ssao_fade_end <= settings.ssao_fade_start) {
        settings.ssao_fade_end = settings.ssao_fade_start + 1.0f;
    }
    settings.ssao_edge_rejection =
        clamp_value(settings.ssao_edge_rejection, 1.05f, 4.0f);
    settings.ssao_highlight_start =
        clamp_value(settings.ssao_highlight_start, 0.0f, 2.0f);
    settings.ssao_highlight_end =
        clamp_value(settings.ssao_highlight_end, 0.01f, 4.0f);
    if (settings.ssao_highlight_end <= settings.ssao_highlight_start) {
        settings.ssao_highlight_end = settings.ssao_highlight_start + 0.01f;
    }
    settings.ssao_highlight_ao_floor =
        clamp_value(settings.ssao_highlight_ao_floor, 0.0f, 1.0f);
    settings.ssao_interior_near_start =
        clamp_value(settings.ssao_interior_near_start, 0.1f, 50.0f);
    settings.ssao_interior_near_end =
        clamp_value(settings.ssao_interior_near_end, 0.2f, 100.0f);
    if (settings.ssao_interior_near_end <= settings.ssao_interior_near_start) {
        settings.ssao_interior_near_end =
            settings.ssao_interior_near_start + 0.1f;
    }
    settings.ssao_interior_radius =
        clamp_value(settings.ssao_interior_radius, 0.05f, 5.0f);
    settings.ssao_interior_intensity =
        clamp_value(settings.ssao_interior_intensity, 0.0f, 1.0f);
    settings.ssao_interior_bias =
        clamp_value(settings.ssao_interior_bias, 0.0f, 0.5f);
    settings.ssao_interior_edge_rejection =
        clamp_value(settings.ssao_interior_edge_rejection, 1.05f, 4.0f);
    settings.temporal_history_weight =
        clamp_value(settings.temporal_history_weight, 0.0f, 0.95f);
    settings.temporal_depth_rejection =
        clamp_value(settings.temporal_depth_rejection, 0.001f, 0.5f);
    settings.temporal_color_rejection =
        clamp_value(settings.temporal_color_rejection, 0.005f, 1.0f);
    return settings;
}

void log_stack(const CalibrationStack& stack, const Settings& settings) {
    log_message(
        "Camadas cumulativas: base_0.1.2=%s visual_0.2.0=%s "
        "rain_overcast_0.3.0=%s.",
        stack.base.enabled ? "ativa" : "inativa",
        stack.visual_0_2.enabled ? "ativa" : "inativa",
        stack.rain_overcast_0_3.enabled ? "ativa" : "inativa");
    log_message(
        "Perfil efetivo: temperature=%.1f exposure=%.3f contrast=%.3f "
        "saturation=%.3f vibrance=%.3f shadows=%.3f highlights=%.3f "
        "blacks=%.3f whites=%.3f local_contrast=%.3f sharpness=%.3f "
        "vignette=%.3f.",
        settings.temperature,
        settings.exposure,
        settings.contrast,
        settings.saturation,
        settings.vibrance,
        settings.shadows,
        settings.highlights,
        settings.blacks,
        settings.whites,
        settings.local_contrast,
        settings.sharpness,
        settings.vignette);
    log_message(
        "Depth linearization 0.6.4: reversed_z=sim near_plane=%.4f "
        "preview_distance=%.1f vertical_fov=%.1f.",
        settings.depth_near_plane,
        settings.depth_preview_distance,
        settings.depth_vertical_fov);
    log_message(
        "Modulo SSAO 0.7.0: %s samples=8 radius=%.3f intensity=%.3f "
        "bias=%.3f fade=%.1f-%.1f edge_rejection=%.2f.",
        settings.ssao_enabled ? "ativo" : "inativo",
        settings.ssao_radius,
        settings.ssao_intensity,
        settings.ssao_bias,
        settings.ssao_fade_start,
        settings.ssao_fade_end,
        settings.ssao_edge_rejection);
    log_message(
        "Modulo SSAO refinement 0.8.0: %s samples=16 "
        "highlight_protection=%.2f-%.2f ao_floor=%.2f.",
        settings.ssao_refinement_enabled ? "ativo" : "inativo",
        settings.ssao_highlight_start,
        settings.ssao_highlight_end,
        settings.ssao_highlight_ao_floor);
    log_message(
        "Modulo SSAO interior 0.9.0: %s faixa=%.1f-%.1fm "
        "radius=%.3f intensity=%.3f bias=%.3f edge_rejection=%.2f.",
        settings.ssao_interior_enabled ? "ativo" : "inativo",
        settings.ssao_interior_near_start,
        settings.ssao_interior_near_end,
        settings.ssao_interior_radius,
        settings.ssao_interior_intensity,
        settings.ssao_interior_bias,
        settings.ssao_interior_edge_rejection);
    log_message(
        "Modulo temporal 0.10.0: %s history_weight=%.2f "
        "depth_rejection=%.3f color_rejection=%.3f.",
        settings.temporal_enabled ? "ativo" : "inativo",
        settings.temporal_history_weight,
        settings.temporal_depth_rejection,
        settings.temporal_color_rejection);
}

}  // namespace

Settings default_settings() {
    return compose_stack(reference_stack());
}

bool load_settings(Settings* settings) {
    if (settings == nullptr) {
        return false;
    }

    CalibrationStack stack = reference_stack();
    FILE* file = _wfopen(config_path(), L"rb");
    if (file == nullptr) {
        *settings = compose_stack(stack);
        log_message("Configuracao ausente; usando a pilha cumulativa interna.");
        log_stack(stack, *settings);
        return false;
    }

    Section section = Section::unknown;
    char line[512] = {};
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        char* content = trim(line);
        if (*content == '\0' || *content == '#' || *content == ';') {
            continue;
        }
        if (*content == '[') {
            char* closing = std::strchr(content + 1, ']');
            if (closing != nullptr) {
                *closing = '\0';
                section = parse_section(trim(content + 1));
            } else {
                section = Section::unknown;
            }
            continue;
        }

        char* separator = std::strchr(content, '=');
        if (separator == nullptr) {
            continue;
        }
        *separator = '\0';
        char* key = trim(content);
        char* value = trim(separator + 1);
        if (section == Section::plugin && _stricmp(key, "enabled") == 0) {
            stack.enabled = parse_bool(value);
            continue;
        }
        if (section == Section::depth_0_6_4) {
            assign_depth_value(&stack, key, value);
            continue;
        }
        if (section == Section::ssao_0_7_0) {
            assign_ssao_value(&stack, key, value);
            continue;
        }
        if (section == Section::ssao_refinement_0_8_0) {
            assign_ssao_refinement_value(&stack, key, value);
            continue;
        }
        if (section == Section::ssao_interior_0_9_0) {
            assign_ssao_interior_value(&stack, key, value);
            continue;
        }
        if (section == Section::temporal_0_10_0) {
            assign_temporal_value(&stack, key, value);
            continue;
        }
        assign_layer_value(layer_for_section(&stack, section), key, value);
    }
    std::fclose(file);

    *settings = compose_stack(stack);
    log_stack(stack, *settings);
    return true;
}

}  // namespace photorealism
