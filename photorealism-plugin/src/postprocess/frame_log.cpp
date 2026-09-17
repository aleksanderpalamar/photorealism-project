#include "frame_log.hpp"

#include "../config/effect_quality.hpp"
#include "../runtime.hpp"

namespace photorealism {
namespace {

void log_bloom_state(
    const Settings& settings,
    const FrameLogInput& input,
    FrameLogState* state) {
    if (input.bloom_active) {
        state->bloom_wait_logged = false;
        if (state->bloom_active_logged_levels != input.bloom_level_count) {
            log_message(
                "Bloom 0.17.0 ativo: niveis=%u threshold=%.3f knee=%.3f "
                "intensity=%.3f radius=%.4f.",
                input.bloom_level_count,
                settings.bloom_threshold,
                settings.bloom_knee,
                settings.bloom_intensity,
                settings.bloom_radius);
            state->bloom_active_logged_levels = input.bloom_level_count;
        }
        return;
    }
    if (settings.bloom_enabled && input.depth_preview_mode == 0 &&
        !state->bloom_wait_logged) {
        log_message(
            "Bloom 0.17.0 aguardando shaders e recursos validos; "
            "a pilha visual aprovada permanece ativa.");
        state->bloom_wait_logged = true;
    }
}

void log_preview_state(
    const Settings& settings,
    const FrameLogInput& input,
    FrameLogState* state) {
    if (input.depth_preview) {
        state->depth_preview_wait_logged = false;
        if (state->depth_preview_logged_mode != input.depth_preview_mode) {
            log_message(
                "Preview depth ativo: mode=%u source=%ux%u "
                "format=%u generation=%llu near=%.4f range=%.1f "
                "vertical_fov=%.1f.",
                input.depth_preview_mode,
                input.depth_description.Width,
                input.depth_description.Height,
                static_cast<unsigned>(input.depth_description.Format),
                static_cast<unsigned long long>(input.depth_generation),
                settings.depth_near_plane,
                settings.depth_preview_distance,
                settings.depth_vertical_fov);
            state->depth_preview_logged_mode = input.depth_preview_mode;
        }
        return;
    }
    if (!input.ssao_preview) {
        return;
    }
    state->depth_preview_wait_logged = false;
    if (state->depth_preview_logged_mode != input.depth_preview_mode) {
        log_message(
            "Preview SSAO ativo: mode=5 source=%ux%u generation=%llu "
            "radius=%.3f intensity=%.3f.",
            input.depth_description.Width,
            input.depth_description.Height,
            static_cast<unsigned long long>(input.depth_generation),
            settings.ssao_radius,
            settings.ssao_intensity);
        state->depth_preview_logged_mode = input.depth_preview_mode;
    }
}

void log_ssao_state(
    const Settings& settings,
    const FrameLogInput& input,
    FrameLogState* state) {
    if (input.ssao_active) {
        state->ssao_wait_logged = false;
        if (state->ssao_active_logged_generation != input.depth_generation) {
            log_message(
                "SSAO 0.23.3 ativo: source=%ux%u format=%u "
                "generation=%llu samples=%u radius=%.3f intensity=%.3f "
                "forca_perfil=%.2f fade=%.1f-%.1f interior=%s.",
                input.depth_description.Width,
                input.depth_description.Height,
                static_cast<unsigned>(input.depth_description.Format),
                static_cast<unsigned long long>(input.depth_generation),
                ssao_quality(settings).samples,
                settings.ssao_radius,
                settings.ssao_intensity,
                ssao_strength(settings),
                settings.ssao_fade_start,
                settings.ssao_fade_end,
                settings.ssao_interior_enabled ? "ativo" : "inativo");
            state->ssao_active_logged_generation = input.depth_generation;
        }
        return;
    }
    if (input.depth_preview_mode == 0 && settings.ssao_enabled &&
        !state->ssao_wait_logged) {
        log_message(
            "SSAO 0.9.1 aguardando depth e recursos validos; "
            "o passe visual aprovado permanece ativo.");
        state->ssao_wait_logged = true;
    }
}

void log_temporal_state(
    const Settings& settings,
    const FrameLogInput& input,
    FrameLogState* state) {
    if (input.temporal_active) {
        state->temporal_wait_logged = false;
        if (state->temporal_active_logged_generation != input.depth_generation) {
            log_message(
                "Resolve temporal 0.10.0 ativo: source=%ux%u "
                "depth=%ux%u generation=%llu history_weight=%.2f "
                "depth_rejection=%.3f color_rejection=%.3f.",
                input.description.Width,
                input.description.Height,
                input.depth_description.Width,
                input.depth_description.Height,
                static_cast<unsigned long long>(input.depth_generation),
                settings.temporal_history_weight,
                settings.temporal_depth_rejection,
                settings.temporal_color_rejection);
            state->temporal_active_logged_generation = input.depth_generation;
        }
        return;
    }
    if (input.depth_preview_mode == 0 && settings.temporal_enabled &&
        !state->temporal_wait_logged) {
        log_message(
            "Resolve temporal 0.10.0 aguardando depth e recursos validos; "
            "pilha visual/SSAO permanece ativa.");
        state->temporal_wait_logged = true;
    }
}

}  // namespace

void log_frame_plan(
    const Settings& settings,
    const FrameLogInput& input,
    FrameLogState* state) {
    log_bloom_state(settings, input, state);
    log_preview_state(settings, input, state);
    log_ssao_state(settings, input, state);
    log_temporal_state(settings, input, state);
}

}  // namespace photorealism
