#include "effect_logging.hpp"

#include "effect_quality.hpp"

#include <cstdio>

namespace photorealism {
namespace {

constexpr const char* kQualityNames[] = {"alta", "media", "baixa"};
constexpr const char* kAntiAliasingNames[] = {
    "desligado", "temporal", "temporal-nitido"};
constexpr const char* kPresetNames[] = {"suave", "medio", "forte"};

const char* yes_no(bool value) {
    return value ? "sim" : "nao";
}

}

void format_effect_settings(const Settings& settings, char* text, std::size_t size) {
    const SsaoQuality ssao = ssao_quality(settings);
    std::snprintf(
        text, size,
        "Efeitos 0.23.3: qualidade=%s aa=%s fxaa=%s ssao_forca=%.2f "
        "ssao_amostras=%u ssao_meia_resolucao=%s ssao_preset=%s "
        "luz_interior=%.2f bloom_niveis_max=%u.",
        kQualityNames[quality_level(settings.profile_global_quality)],
        kAntiAliasingNames[static_cast<unsigned>(anti_aliasing_mode(settings))],
        yes_no(fxaa_enabled(settings)),
        static_cast<double>(ssao_strength(settings)), ssao.samples,
        yes_no(ssao.half_resolution),
        kPresetNames[quality_level(settings.profile_ssao_preset)],
        static_cast<double>(interior_light_strength(settings)),
        bloom_level_limit(settings));
}

}
