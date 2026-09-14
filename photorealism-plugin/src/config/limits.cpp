#include "limits.hpp"

#include "text_utils.hpp"

namespace photorealism {
namespace {

struct Limit {
    float Settings::*member;
    float minimum;
    float maximum;
};

constexpr Limit kLimits[] = {
    {&Settings::temperature, 3000.0f, 9000.0f},
    {&Settings::exposure, -2.0f, 2.0f},
    {&Settings::contrast, 0.5f, 1.5f},
    {&Settings::saturation, 0.0f, 2.0f},
    {&Settings::vibrance, -1.0f, 1.0f},
    {&Settings::shadows, -1.0f, 1.0f},
    {&Settings::highlights, -1.0f, 1.0f},
    {&Settings::blacks, -1.0f, 1.0f},
    {&Settings::whites, -1.0f, 1.0f},
    {&Settings::local_contrast, 0.0f, 1.0f},
    {&Settings::sharpness, 0.0f, 1.0f},
    {&Settings::vignette, 0.0f, 0.5f},
    {&Settings::depth_near_plane, 0.001f, 10.0f},
    {&Settings::depth_preview_distance, 1.0f, 10000.0f},
    {&Settings::depth_vertical_fov, 20.0f, 140.0f},
    {&Settings::ssao_radius, 0.05f, 5.0f},
    {&Settings::ssao_intensity, 0.0f, 1.0f},
    {&Settings::ssao_bias, 0.0f, 0.5f},
    {&Settings::ssao_fade_start, 1.0f, 500.0f},
    {&Settings::ssao_fade_end, 2.0f, 1000.0f},
    {&Settings::ssao_edge_rejection, 1.05f, 4.0f},
    {&Settings::ssao_highlight_start, 0.0f, 2.0f},
    {&Settings::ssao_highlight_end, 0.01f, 4.0f},
    {&Settings::ssao_highlight_ao_floor, 0.0f, 1.0f},
    {&Settings::ssao_interior_near_start, 0.1f, 50.0f},
    {&Settings::ssao_interior_near_end, 0.2f, 100.0f},
    {&Settings::ssao_interior_radius, 0.05f, 5.0f},
    {&Settings::ssao_interior_intensity, 0.0f, 1.0f},
    {&Settings::ssao_interior_bias, 0.0f, 0.5f},
    {&Settings::ssao_interior_edge_rejection, 1.05f, 4.0f},
    {&Settings::temporal_history_weight, 0.0f, 0.95f},
    {&Settings::temporal_depth_rejection, 0.001f, 0.5f},
    {&Settings::temporal_color_rejection, 0.005f, 1.0f},

    {&Settings::bloom_threshold, 0.2f, 0.98f},
    {&Settings::bloom_knee, 0.0f, 0.5f},
    {&Settings::bloom_intensity, 0.0f, 1.0f},
    {&Settings::bloom_radius, 0.005f, 0.2f},

    {&Settings::fsr_render_scale, 0.50f, 1.0f},
    {&Settings::fsr_sharpness, 0.0f, 1.0f},
    {&Settings::fsr_grain, 0.0f, 1.0f},

    {&Settings::scene_observer_interval_frames, 1.0f, 600.0f},
    {&Settings::scene_observer_log_seconds, 0.0f, 3600.0f},
    {&Settings::condition_time_constant_seconds, 1.0f, 1800.0f},
    {&Settings::condition_log_seconds, 0.0f, 3600.0f},
    {&Settings::condition_minimum_dynamic_range, 0.0f, 255.0f},

    {&Settings::condition_sun_temperature, 3000.0f, 9000.0f},
    {&Settings::condition_rain_temperature, 3000.0f, 9000.0f},
    {&Settings::condition_night_temperature, 3000.0f, 9000.0f},
    {&Settings::condition_sun_tint, -1.0f, 1.0f},
    {&Settings::condition_rain_tint, -1.0f, 1.0f},
    {&Settings::condition_night_tint, -1.0f, 1.0f},
};

struct OrderedPair {
    float Settings::*low;
    float Settings::*high;
    float minimum_gap;
};

constexpr OrderedPair kOrderedPairs[] = {
    {&Settings::ssao_fade_start, &Settings::ssao_fade_end, 1.0f},
    {&Settings::ssao_highlight_start, &Settings::ssao_highlight_end, 0.01f},
    {&Settings::ssao_interior_near_start, &Settings::ssao_interior_near_end,
     0.1f},
    {&Settings::condition_daylight_median_low,
     &Settings::condition_daylight_median_high, 1.0f},
    {&Settings::condition_overcast_saturation_low,
     &Settings::condition_overcast_saturation_high, 0.01f},
};

}

void apply_limits(Settings* settings) {
    for (const Limit& limit : kLimits) {
        settings->*(limit.member) =
            config_text::clamp_value(settings->*(limit.member), limit.minimum, limit.maximum);
    }
    for (const OrderedPair& pair : kOrderedPairs) {
        if (settings->*(pair.high) > settings->*(pair.low)) {
            continue;
        }
        settings->*(pair.high) = settings->*(pair.low) + pair.minimum_gap;
    }
}
}
