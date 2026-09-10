#include "postprocess.hpp"

#include "../config.hpp"
#include "../resource_observer.hpp"
#include "../runtime.hpp"
#include "../scene_conditions.hpp"
#include "../scene_observer.hpp"
#include "../steam_screenshots.hpp"
#include "com_utils.hpp"
#include "device_state.hpp"
#include "gpu_timer.hpp"
#include "shader_constants.hpp"
#include "bloom_pyramid.hpp"
#include "depth_capture.hpp"
#include "format_utils.hpp"
#include "shader_library.hpp"
#include "temporal_history.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <windows.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace photorealism {
namespace {

bool key_pressed_once(int virtual_key, bool* was_down) {
    if (was_down == nullptr) {
        return false;
    }
    const SHORT state = GetAsyncKeyState(virtual_key);
    const bool is_down = (state & 0x8000) != 0;
    const bool pressed = (state & 1) != 0 || (is_down && !*was_down);
    *was_down = is_down;
    return pressed;
}

class PostProcessor {
public:
    PostProcessor() : settings_(default_settings()) {}

    struct FrameTargets {
        ID3D11Texture2D* back_buffer;
        ID3D11RenderTargetView* output;
        D3D11_TEXTURE2D_DESC description;
        bool output_needs_srgb_encode;
    };

    struct DepthState {
        bool available;
        D3D11_TEXTURE2D_DESC description;
        std::uint64_t generation;
    };

    struct FramePlan {
        DepthState depth;
        bool depth_preview;
        bool ssao_preview;
        bool bloom_preview;
        bool ssao;
        bool temporal;
        bool bloom;
    };

    void track_active_swap_chain(IDXGISwapChain* swap_chain) {
        if (active_swap_chain_ == swap_chain) {
            return;
        }
        release_frame_resources();
        active_swap_chain_ = swap_chain;
        log_message(
            "Swap chain ativo detectado: %p; recursos serao vinculados "
            "no proximo frame.",
            static_cast<void*>(swap_chain));
    }

    static const char* depth_preview_mode_name(unsigned mode) {
        static const char* const kNames[] = {
            "normal",
            "raw",
            "reversed-z-enhanced",
            "linear-distance",
            "reconstructed-normals",
            "ssao-visibility",
            "bloom",
        };
        const unsigned count = sizeof(kNames) / sizeof(kNames[0]);
        return mode < count ? kNames[mode] : "normal";
    }

    void toggle_effect() {
        settings_.enabled = !settings_.enabled;
        invalidate_temporal_history("alternancia Home");
        log_message(
            "Efeito %s pelo atalho Home.",
            settings_.enabled ? "ativado" : "desativado");
    }

    void reload_configuration() {
        load_settings(&settings_);
        apply_scene_observer_settings();
        if (device_ != nullptr) {
            recompile_shaders();
        }
        release_depth_capture_resources();
        restart_depth_discovery();
        log_message(
            "Configuracao, shader e descoberta depth recarregados "
            "pelo atalho End.");
    }

    void cycle_depth_preview_mode() {
        depth_preview_mode_ = (depth_preview_mode_ + 1) % 7;
        invalidate_temporal_history("mudanca de preview Insert");
        depth_preview_wait_logged_ = false;
        depth_preview_logged_mode_ = 0;
        log_message(
            "Preview depth solicitado pelo Insert: mode=%u(%s).",
            depth_preview_mode_,
            depth_preview_mode_name(depth_preview_mode_));
    }

    void handle_hotkeys() {
        if (key_pressed_once(VK_HOME, &home_key_down_)) {
            toggle_effect();
        }
        if (key_pressed_once(VK_END, &end_key_down_)) {
            reload_configuration();
        }
        if (key_pressed_once(VK_INSERT, &insert_key_down_)) {
            cycle_depth_preview_mode();
        }
    }

    bool adopt_device(IDXGISwapChain* swap_chain, ID3D11Device* frame_device) {
        const bool replacing_device = device_ != nullptr;
        reset_device();
        if (replacing_device) {
            restart_depth_discovery_for_device_change();
        }
        active_swap_chain_ = swap_chain;
        device_ = frame_device;
        device_->AddRef();
        device_->GetImmediateContext(&context_);
        load_settings(&settings_);
        apply_scene_observer_settings();
        if (!initialize_pipeline()) {
            return false;
        }
        log_message("Pipeline D3D11 inicializado.");
        return true;
    }

    bool ensure_device_for(IDXGISwapChain* swap_chain) {
        ID3D11Device* frame_device = nullptr;
        const HRESULT result = swap_chain->GetDevice(
            IID_ID3D11Device, reinterpret_cast<void**>(&frame_device));
        if (FAILED(result) || frame_device == nullptr) {
            return false;
        }
        const bool ready =
            device_ == frame_device || adopt_device(swap_chain, frame_device);
        safe_release(frame_device);
        return ready;
    }

    bool backbuffer_is_supported(const D3D11_TEXTURE2D_DESC& description) {
        const bool supported =
            description.Width >= 640 && description.Height >= 480 &&
            description.SampleDesc.Count == 1 &&
            is_supported_format(description.Format);
        if (supported || unsupported_logged_) {
            return supported;
        }
        log_message(
            "Backbuffer ignorado: %ux%u format=%u samples=%u.",
            description.Width,
            description.Height,
            static_cast<unsigned>(description.Format),
            description.SampleDesc.Count);
        unsupported_logged_ = true;
        return false;
    }

    bool create_output_view(FrameTargets* targets) {
        D3D11_RENDER_TARGET_VIEW_DESC output_description = {};
        output_description.Format =
            srgb_view_format(targets->description.Format);
        output_description.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        output_description.Texture2D.MipSlice = 0;
        HRESULT result = device_->CreateRenderTargetView(
            targets->back_buffer, &output_description, &targets->output);
        if (SUCCEEDED(result) && targets->output != nullptr) {
            return true;
        }

        result = device_->CreateRenderTargetView(
            targets->back_buffer, nullptr, &targets->output);
        targets->output_needs_srgb_encode =
            is_unorm_format(targets->description.Format);
        if (!output_fallback_logged_) {
            log_message(
                "RTV sRGB indisponivel; usando codificacao manual na saida.");
            output_fallback_logged_ = true;
        }
        if (SUCCEEDED(result) && targets->output != nullptr) {
            return true;
        }
        log_message(
            "Falha ao criar RTV do backbuffer: 0x%08X.",
            static_cast<unsigned>(result));
        return false;
    }

    bool acquire_frame_targets(
        IDXGISwapChain* swap_chain, FrameTargets* targets) {
        const HRESULT result = swap_chain->GetBuffer(
            0,
            IID_ID3D11Texture2D,
            reinterpret_cast<void**>(&targets->back_buffer));
        if (FAILED(result) || targets->back_buffer == nullptr) {
            return false;
        }

        targets->back_buffer->GetDesc(&targets->description);
        if (!backbuffer_is_supported(targets->description)) {
            return false;
        }

        update_backbuffer_signature(
            targets->description.Width,
            targets->description.Height,
            targets->description.Format);

        if (!ensure_frame_resources(targets->description)) {
            return false;
        }
        return create_output_view(targets);
    }

    void release_frame_targets(FrameTargets* targets) {
        safe_release(targets->output);
        safe_release(targets->back_buffer);
    }

    void note_depth_active(std::uint64_t generation, std::uint64_t serial) {
        if (depth_stale_logged_) {
            log_message(
                "Depth voltou a ser usado pela cena: generation=%llu; "
                "SSAO photorealista retomado.",
                static_cast<unsigned long long>(generation));
        }
        depth_last_binding_serial_ = serial;
        depth_stale_frame_count_ = 0;
        depth_stale_logged_ = false;
    }

    bool note_depth_idle(std::uint64_t generation, std::uint64_t serial) {
        if (depth_stale_frame_count_ < kDepthStaleFrameThreshold) {
            ++depth_stale_frame_count_;
        }
        const bool grace_just_expired =
            depth_stale_frame_count_ == kDepthActivityGraceFrames + 1 &&
            !depth_stale_logged_;
        if (grace_just_expired) {
            log_message(
                "Depth sem atividade confirmada por %u frames: "
                "generation=%llu; SSAO suspenso e visual "
                "photorealista preservado.",
                depth_stale_frame_count_,
                static_cast<unsigned long long>(generation));
            depth_stale_logged_ = true;
        }

        const bool expired =
            depth_stale_frame_count_ >= kDepthStaleFrameThreshold &&
            invalidate_stale_depth_candidate(generation, serial);
        if (!expired) {
            return false;
        }
        log_message(
            "Depth obsoleto invalidado apos %u frames; "
            "redescoberta automatica iniciada sem acao do usuario.",
            kDepthStaleFrameThreshold);
        release_depth_capture_resources();
        return true;
    }

    bool depth_is_safe_for_scene(
        std::uint64_t generation, std::uint64_t serial) {
        if (depth_liveness_generation_ != generation) {
            depth_liveness_generation_ = generation;
            depth_last_binding_serial_ = 0;
            depth_stale_frame_count_ = 0;
            depth_stale_logged_ = false;
        }

        const bool used_by_current_scene =
            serial != 0 && serial != depth_last_binding_serial_;
        if (used_by_current_scene) {
            note_depth_active(generation, serial);
            return true;
        }
        if (note_depth_idle(generation, serial)) {
            return false;
        }
        return depth_stale_frame_count_ <= kDepthActivityGraceFrames;
    }

    bool depth_is_requested() const {
        return settings_.ssao_enabled || settings_.temporal_enabled ||
               (depth_preview_mode_ != 0 && depth_preview_mode_ != 6);
    }

    DepthState acquire_depth_state() {
        DepthState depth = {};
        if (!depth_is_requested()) {
            return depth;
        }

        ID3D11Texture2D* candidate = nullptr;
        std::uint64_t serial = 0;
        if (!acquire_depth_candidate(
                &candidate, &depth.description, &depth.generation, &serial)) {
            safe_release(candidate);
            return depth;
        }

        const bool safe = depth_is_safe_for_scene(depth.generation, serial);
        depth.available =
            safe && depth_capture_.ensure(
                        candidate,
                        depth.description,
                        depth.generation,
                        shaders_.depth_preview() != nullptr ||
                            shaders_.ssao() != nullptr);
        if (depth.available) {
            depth_capture_.copy_from(candidate);
        }
        safe_release(candidate);
        return depth;
    }

    bool plan_temporal(
        const D3D11_TEXTURE2D_DESC& description,
        const DepthState& depth,
        bool ssao_active) {
        const bool eligible =
            depth.available && depth_preview_mode_ == 0 &&
            settings_.temporal_enabled && shaders_.temporal() != nullptr &&
            visual_target_ != nullptr && visual_view_ != nullptr;
        if (!eligible) {
            return false;
        }
        if (!temporal_.ensure(
                description,
                depth.description,
                depth.generation,
                shaders_.temporal() != nullptr,
                depth_capture_.texture())) {
            return false;
        }
        const bool spatial_missing =
            spatial_target_ == nullptr || spatial_view_ == nullptr;
        return !(ssao_active && spatial_missing);
    }

    bool plan_bloom(
        const D3D11_TEXTURE2D_DESC& description, bool bloom_preview) {
        const bool eligible =
            (depth_preview_mode_ == 0 || bloom_preview) &&
            settings_.bloom_enabled && shaders_.bloom_bright() != nullptr &&
            additive_blend_state_ != nullptr;
        if (!eligible) {
            return false;
        }
        return bloom_.ensure(
            description,
            BloomPyramid::levels_for_radius(
                settings_.bloom_radius, description.Height));
    }

    FramePlan plan_frame(const D3D11_TEXTURE2D_DESC& description) {
        FramePlan plan = {};
        plan.depth = acquire_depth_state();

        plan.depth_preview =
            plan.depth.available && depth_preview_mode_ >= 1 &&
            depth_preview_mode_ <= 4 && shaders_.depth_preview() != nullptr;
        plan.ssao_preview =
            plan.depth.available && depth_preview_mode_ == 5 &&
            shaders_.ssao() != nullptr;
        plan.ssao =
            plan.depth.available && depth_preview_mode_ == 0 &&
            settings_.ssao_enabled && shaders_.ssao() != nullptr &&
            visual_target_ != nullptr && visual_view_ != nullptr;
        plan.temporal = plan_temporal(description, plan.depth, plan.ssao);

        if (!plan.temporal && temporal_.valid()) {
            invalidate_temporal_history("depth ou passe temporal indisponivel");
        }

        plan.bloom_preview =
            depth_preview_mode_ == 6 && shaders_.bloom_bright() != nullptr &&
            additive_blend_state_ != nullptr;
        plan.bloom = plan_bloom(description, plan.bloom_preview);
        if (!plan.bloom) {
            plan.bloom_preview = false;
        }
        return plan;
    }

    void log_bloom_state(const FramePlan& plan) {
        if (plan.bloom) {
            bloom_wait_logged_ = false;
            if (bloom_active_logged_levels_ != bloom_.level_count()) {
                log_message(
                    "Bloom 0.17.0 ativo: niveis=%u threshold=%.3f knee=%.3f "
                    "intensity=%.3f radius=%.4f.",
                    bloom_.level_count(),
                    settings_.bloom_threshold,
                    settings_.bloom_knee,
                    settings_.bloom_intensity,
                    settings_.bloom_radius);
                bloom_active_logged_levels_ = bloom_.level_count();
            }
            return;
        }
        if (settings_.bloom_enabled && depth_preview_mode_ == 0 &&
            !bloom_wait_logged_) {
            log_message(
                "Bloom 0.17.0 aguardando shaders e recursos validos; "
                "a pilha visual aprovada permanece ativa.");
            bloom_wait_logged_ = true;
        }
    }

    void log_preview_state(const FramePlan& plan) {
        if (plan.depth_preview) {
            depth_preview_wait_logged_ = false;
            if (depth_preview_logged_mode_ != depth_preview_mode_) {
                log_message(
                    "Preview depth ativo: mode=%u source=%ux%u "
                    "format=%u generation=%llu near=%.4f range=%.1f "
                    "vertical_fov=%.1f.",
                    depth_preview_mode_,
                    plan.depth.description.Width,
                    plan.depth.description.Height,
                    static_cast<unsigned>(plan.depth.description.Format),
                    static_cast<unsigned long long>(plan.depth.generation),
                    settings_.depth_near_plane,
                    settings_.depth_preview_distance,
                    settings_.depth_vertical_fov);
                depth_preview_logged_mode_ = depth_preview_mode_;
            }
            return;
        }
        if (!plan.ssao_preview) {
            return;
        }
        depth_preview_wait_logged_ = false;
        if (depth_preview_logged_mode_ != depth_preview_mode_) {
            log_message(
                "Preview SSAO ativo: mode=5 source=%ux%u generation=%llu "
                "radius=%.3f intensity=%.3f.",
                plan.depth.description.Width,
                plan.depth.description.Height,
                static_cast<unsigned long long>(plan.depth.generation),
                settings_.ssao_radius,
                settings_.ssao_intensity);
            depth_preview_logged_mode_ = depth_preview_mode_;
        }
    }

    void log_ssao_state(const FramePlan& plan) {
        if (plan.ssao) {
            ssao_wait_logged_ = false;
            if (ssao_active_logged_generation_ != plan.depth.generation) {
                log_message(
                    "SSAO 0.9.1 ativo: source=%ux%u format=%u "
                    "generation=%llu samples=%u radius=%.3f intensity=%.3f "
                    "fade=%.1f-%.1f interior=%s.",
                    plan.depth.description.Width,
                    plan.depth.description.Height,
                    static_cast<unsigned>(plan.depth.description.Format),
                    static_cast<unsigned long long>(plan.depth.generation),
                    settings_.ssao_refinement_enabled ? 16u : 8u,
                    settings_.ssao_radius,
                    settings_.ssao_intensity,
                    settings_.ssao_fade_start,
                    settings_.ssao_fade_end,
                    settings_.ssao_interior_enabled ? "ativo" : "inativo");
                ssao_active_logged_generation_ = plan.depth.generation;
            }
            return;
        }
        if (depth_preview_mode_ == 0 && settings_.ssao_enabled &&
            !ssao_wait_logged_) {
            log_message(
                "SSAO 0.9.1 aguardando depth e recursos validos; "
                "o passe visual aprovado permanece ativo.");
            ssao_wait_logged_ = true;
        }
    }

    void log_temporal_state(
        const FramePlan& plan, const D3D11_TEXTURE2D_DESC& description) {
        if (plan.temporal) {
            temporal_wait_logged_ = false;
            if (temporal_active_logged_generation_ != plan.depth.generation) {
                log_message(
                    "Resolve temporal 0.10.0 ativo: source=%ux%u "
                    "depth=%ux%u generation=%llu history_weight=%.2f "
                    "depth_rejection=%.3f color_rejection=%.3f.",
                    description.Width,
                    description.Height,
                    plan.depth.description.Width,
                    plan.depth.description.Height,
                    static_cast<unsigned long long>(plan.depth.generation),
                    settings_.temporal_history_weight,
                    settings_.temporal_depth_rejection,
                    settings_.temporal_color_rejection);
                temporal_active_logged_generation_ = plan.depth.generation;
            }
            return;
        }
        if (depth_preview_mode_ == 0 && settings_.temporal_enabled &&
            !temporal_wait_logged_) {
            log_message(
                "Resolve temporal 0.10.0 aguardando depth e recursos validos; "
                "pilha visual/SSAO permanece ativa.");
            temporal_wait_logged_ = true;
        }
    }

    void log_frame_plan(
        const FramePlan& plan, const D3D11_TEXTURE2D_DESC& description) {
        log_bloom_state(plan);
        log_preview_state(plan);
        log_ssao_state(plan);
        log_temporal_state(plan, description);
    }

    void capture_scene_for_grade(
        const FramePlan& plan, ID3D11Texture2D* back_buffer) {
        if (plan.depth_preview) {
            return;
        }
        context_->CopyResource(scene_texture_, back_buffer);

        scene_observer_.observe(device_, context_, scene_texture_);

        update_condition_adaptation();

        const bool diagnostic_waiting =
            depth_preview_mode_ != 0 && !plan.ssao_preview &&
            !plan.bloom_preview && !depth_preview_wait_logged_;
        if (!diagnostic_waiting) {
            return;
        }
        log_message(
            "Diagnostico depth/SSAO aguardando candidato valido; "
            "o passe visual normal permanece ativo.");
        depth_preview_wait_logged_ = true;
    }

    struct ProjectionScale {
        float x;
        float y;
    };

    ProjectionScale projection_scale_for(
        const D3D11_TEXTURE2D_DESC& description) const {
        constexpr float pi = 3.14159265358979323846f;
        const float vertical_fov_radians =
            settings_.depth_vertical_fov * pi / 180.0f;
        const float y = 1.0f / std::tan(vertical_fov_radians * 0.5f);
        const float aspect =
            static_cast<float>(description.Width) /
            static_cast<float>(description.Height);
        return ProjectionScale{y / aspect, y};
    }

    void upload_visual_constants(
        const FrameTargets& targets, const FramePlan& plan) {
        ShaderConstants constants = {};
        constants.texel_size[0] =
            1.0f / static_cast<float>(targets.description.Width);
        constants.texel_size[1] =
            1.0f / static_cast<float>(targets.description.Height);
        constants.exposure = settings_.exposure;
        constants.temperature = effective_temperature_;
        constants.contrast = settings_.contrast;
        constants.saturation = settings_.saturation;
        constants.vibrance = settings_.vibrance;
        constants.shadows = settings_.shadows;
        constants.highlights = settings_.highlights;
        constants.blacks = settings_.blacks;
        constants.whites = settings_.whites;
        constants.local_contrast = settings_.local_contrast;
        constants.sharpness = settings_.sharpness;
        constants.vignette = settings_.vignette;
        constants.black_lift[0] = settings_.black_lift_r;
        constants.black_lift[1] = settings_.black_lift_g;
        constants.black_lift[2] = settings_.black_lift_b;
        constants.highlight_rolloff = settings_.highlight_rolloff;
        constants.tint = effective_tint_;
        constants.bloom_enabled = plan.bloom ? 1.0f : 0.0f;
        constants.bloom_intensity = settings_.bloom_intensity;
        constants.input_needs_srgb_decode =
            scene_needs_srgb_decode_ ? 1.0f : 0.0f;
        const bool visual_writes_to_output = !plan.ssao && !plan.temporal;
        constants.output_needs_srgb_encode =
            visual_writes_to_output && targets.output_needs_srgb_encode ? 1.0f
                                                                       : 0.0f;
        context_->UpdateSubresource(
            constant_buffer_, 0, nullptr, &constants, 0, 0);
    }

    void upload_depth_constants(
        const FrameTargets& targets,
        const ProjectionScale& projection,
        float depth_texel_x,
        float depth_texel_y) {
        DepthPreviewConstants depth_constants = {};
        depth_constants.preview_mode =
            static_cast<float>(depth_preview_mode_);
        depth_constants.output_needs_srgb_encode =
            targets.output_needs_srgb_encode ? 1.0f : 0.0f;
        depth_constants.near_plane = settings_.depth_near_plane;
        depth_constants.preview_distance = settings_.depth_preview_distance;
        depth_constants.texel_size[0] = depth_texel_x;
        depth_constants.texel_size[1] = depth_texel_y;
        depth_constants.projection_scale[0] = projection.x;
        depth_constants.projection_scale[1] = projection.y;
        context_->UpdateSubresource(
            depth_constant_buffer_, 0, nullptr, &depth_constants, 0, 0);
    }

    void upload_ssao_constants(
        const FrameTargets& targets,
        const FramePlan& plan,
        const ProjectionScale& projection,
        float depth_texel_x,
        float depth_texel_y) {
        SsaoConstants ssao_constants = {};
        ssao_constants.input_needs_srgb_decode =
            plan.ssao_preview && scene_needs_srgb_decode_ ? 1.0f : 0.0f;
        const bool ssao_writes_to_output = !plan.temporal;
        ssao_constants.output_needs_srgb_encode =
            ssao_writes_to_output && targets.output_needs_srgb_encode ? 1.0f
                                                                     : 0.0f;
        ssao_constants.near_plane = settings_.depth_near_plane;
        ssao_constants.radius = settings_.ssao_radius;
        ssao_constants.intensity = settings_.ssao_intensity;
        ssao_constants.bias = settings_.ssao_bias;
        ssao_constants.fade_start = settings_.ssao_fade_start;
        ssao_constants.fade_end = settings_.ssao_fade_end;
        ssao_constants.edge_rejection = settings_.ssao_edge_rejection;
        ssao_constants.debug_mode = plan.ssao_preview ? 1.0f : 0.0f;
        ssao_constants.depth_texel_size[0] = depth_texel_x;
        ssao_constants.depth_texel_size[1] = depth_texel_y;
        ssao_constants.projection_scale[0] = projection.x;
        ssao_constants.projection_scale[1] = projection.y;
        ssao_constants.refinement_enabled =
            settings_.ssao_refinement_enabled ? 1.0f : 0.0f;
        ssao_constants.highlight_start = settings_.ssao_highlight_start;
        ssao_constants.highlight_end = settings_.ssao_highlight_end;
        ssao_constants.highlight_ao_floor =
            settings_.ssao_highlight_ao_floor;
        ssao_constants.interior_enabled =
            settings_.ssao_interior_enabled ? 1.0f : 0.0f;
        ssao_constants.interior_near_start =
            settings_.ssao_interior_near_start;
        ssao_constants.interior_near_end = settings_.ssao_interior_near_end;
        ssao_constants.interior_radius = settings_.ssao_interior_radius;
        ssao_constants.interior_intensity = settings_.ssao_interior_intensity;
        ssao_constants.interior_bias = settings_.ssao_interior_bias;
        ssao_constants.interior_edge_rejection =
            settings_.ssao_interior_edge_rejection;
        context_->UpdateSubresource(
            ssao_constant_buffer_, 0, nullptr, &ssao_constants, 0, 0);
    }

    void upload_temporal_constants(const FrameTargets& targets) {
        TemporalConstants temporal_constants = {};
        temporal_constants.texel_size[0] =
            1.0f / static_cast<float>(targets.description.Width);
        temporal_constants.texel_size[1] =
            1.0f / static_cast<float>(targets.description.Height);
        temporal_constants.near_plane = settings_.depth_near_plane;
        temporal_constants.history_weight =
            settings_.temporal_history_weight;
        temporal_constants.depth_rejection =
            settings_.temporal_depth_rejection;
        temporal_constants.color_rejection =
            settings_.temporal_color_rejection;
        temporal_constants.history_valid =
            temporal_.valid() ? 1.0f : 0.0f;
        temporal_constants.output_needs_srgb_encode =
            targets.output_needs_srgb_encode ? 1.0f : 0.0f;
        context_->UpdateSubresource(
            temporal_constant_buffer_,
            0,
            nullptr,
            &temporal_constants,
            0,
            0);
    }

    void upload_frame_constants(
        const FrameTargets& targets, const FramePlan& plan) {
        const D3D11_TEXTURE2D_DESC& depth_source =
            plan.depth.available ? plan.depth.description : targets.description;
        const float depth_texel_x =
            1.0f / static_cast<float>(depth_source.Width);
        const float depth_texel_y =
            1.0f / static_cast<float>(depth_source.Height);
        const ProjectionScale projection =
            projection_scale_for(targets.description);

        upload_visual_constants(targets, plan);
        upload_depth_constants(
            targets, projection, depth_texel_x, depth_texel_y);
        upload_ssao_constants(
            targets, plan, projection, depth_texel_x, depth_texel_y);
        upload_temporal_constants(targets);
    }

    BloomFrame bloom_frame() const {
        BloomFrame frame = {};
        frame.shaders = &shaders_;
        frame.sampler = sampler_state_;
        frame.scene_view = scene_view_;
        frame.additive_blend = additive_blend_state_;
        frame.opaque_blend = blend_state_;
        frame.scene_needs_srgb_decode = scene_needs_srgb_decode_;
        frame.threshold = settings_.bloom_threshold;
        frame.knee = settings_.bloom_knee;
        return frame;
    }

    void bind_common_pipeline_state(
        const D3D11_TEXTURE2D_DESC& description) {
        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(description.Width);
        viewport.Height = static_cast<float>(description.Height);
        viewport.MaxDepth = 1.0f;

        context_->OMSetBlendState(blend_state_, nullptr, 0xFFFFFFFFu);
        context_->OMSetDepthStencilState(depth_state_, 0);
        context_->RSSetState(rasterizer_state_);
        context_->RSSetViewports(1, &viewport);
        context_->IASetInputLayout(nullptr);
        context_->IASetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(shaders_.vertex(), nullptr, 0);
        context_->GSSetShader(nullptr, nullptr, 0);
        context_->HSSetShader(nullptr, nullptr, 0);
        context_->DSSetShader(nullptr, nullptr, 0);
    }

    void draw_visual_pass(
        ID3D11RenderTargetView* target, const FramePlan& plan) {
        context_->OMSetRenderTargets(1, &target, nullptr);
        context_->PSSetShader(shaders_.visual(), nullptr, 0);
        ID3D11ShaderResourceView* visual_resources[2] = {
            scene_view_, plan.bloom ? bloom_.top_view() : nullptr};
        context_->PSSetShaderResources(0, 2, visual_resources);
        context_->PSSetSamplers(0, 1, &sampler_state_);
        context_->PSSetConstantBuffers(0, 1, &constant_buffer_);
        context_->Draw(3, 0);
    }

    void draw_ssao_pass(
        ID3D11RenderTargetView* target, ID3D11ShaderResourceView* source) {
        context_->OMSetRenderTargets(1, &target, nullptr);
        context_->PSSetShader(shaders_.ssao(), nullptr, 0);
        ID3D11ShaderResourceView* ssao_resources[2] = {
            source, depth_capture_.view()};
        ID3D11SamplerState* ssao_samplers[2] = {
            sampler_state_, depth_sampler_state_};
        context_->PSSetShaderResources(0, 2, ssao_resources);
        context_->PSSetSamplers(0, 2, ssao_samplers);
        context_->PSSetConstantBuffers(0, 1, &ssao_constant_buffer_);
        context_->Draw(3, 0);
    }


    void draw_depth_preview(const FrameTargets& targets) {
        context_->OMSetRenderTargets(1, &targets.output, nullptr);
        context_->PSSetShader(shaders_.depth_preview(), nullptr, 0);
        ID3D11ShaderResourceView* depth_view = depth_capture_.view();
        context_->PSSetShaderResources(0, 1, &depth_view);
        context_->PSSetSamplers(0, 1, &depth_sampler_state_);
        context_->PSSetConstantBuffers(0, 1, &depth_constant_buffer_);
        context_->Draw(3, 0);
    }

    void draw_temporal_chain(
        const FrameTargets& targets, const FramePlan& plan) {
        draw_visual_pass(visual_target_, plan);

        context_->OMSetRenderTargets(0, nullptr, nullptr);
        ID3D11ShaderResourceView* current_view = visual_view_;
        if (plan.ssao) {
            draw_ssao_pass(spatial_target_, visual_view_);
            context_->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11ShaderResourceView* null_ssao_resources[2] = {};
            context_->PSSetShaderResources(0, 2, null_ssao_resources);
            current_view = spatial_view_;
        }

        context_->OMSetRenderTargets(1, &targets.output, nullptr);
        context_->PSSetShader(shaders_.temporal(), nullptr, 0);
        ID3D11ShaderResourceView* temporal_resources[4] = {
            current_view,
            temporal_.valid() ? temporal_.color_view() : nullptr,
            depth_capture_.view(),
            temporal_.valid() ? temporal_.depth_view() : nullptr};
        ID3D11SamplerState* temporal_samplers[2] = {
            sampler_state_, depth_sampler_state_};
        context_->PSSetShaderResources(0, 4, temporal_resources);
        context_->PSSetSamplers(0, 2, temporal_samplers);
        context_->PSSetConstantBuffers(0, 1, &temporal_constant_buffer_);
        context_->Draw(3, 0);

        context_->OMSetRenderTargets(0, nullptr, nullptr);
        ID3D11ShaderResourceView* null_temporal_resources[4] = {};
        context_->PSSetShaderResources(0, 4, null_temporal_resources);
        temporal_.store(targets.back_buffer, depth_capture_.texture());
        if (temporal_.valid()) {
            return;
        }
        temporal_.mark_valid();
        log_message(
            "Historico temporal 0.10.0 inicializado: color=%ux%u "
            "depth=%ux%u generation=%llu.",
            targets.description.Width,
            targets.description.Height,
            plan.depth.description.Width,
            plan.depth.description.Height,
            static_cast<unsigned long long>(plan.depth.generation));
    }

    void draw_ssao_chain(const FrameTargets& targets, const FramePlan& plan) {
        draw_visual_pass(visual_target_, plan);
        context_->OMSetRenderTargets(0, nullptr, nullptr);
        draw_ssao_pass(targets.output, visual_view_);
    }

    void compose_output(const FrameTargets& targets, const FramePlan& plan) {
        if (plan.bloom_preview) {
            bloom_.draw_preview(
                targets.output,
                bloom_frame(),
                targets.output_needs_srgb_encode);
            return;
        }
        if (plan.depth_preview) {
            draw_depth_preview(targets);
            return;
        }
        if (plan.ssao_preview) {
            draw_ssao_pass(targets.output, scene_view_);
            return;
        }
        if (plan.temporal) {
            draw_temporal_chain(targets, plan);
            return;
        }
        if (plan.ssao) {
            draw_ssao_chain(targets, plan);
            return;
        }
        draw_visual_pass(targets.output, plan);
    }

    void log_first_processed_frame(const FrameTargets& targets) {
        if (processed_logged_) {
            return;
        }
        log_message(
            "Primeiro frame processado: %ux%u format=%u "
            "srgb_manual_entrada=%s srgb_manual_saida=%s "
            "ssao_solicitado=%s temporal_solicitado=%s.",
            targets.description.Width,
            targets.description.Height,
            static_cast<unsigned>(targets.description.Format),
            scene_needs_srgb_decode_ ? "sim" : "nao",
            targets.output_needs_srgb_encode ? "sim" : "nao",
            settings_.ssao_enabled ? "sim" : "nao",
            settings_.temporal_enabled ? "sim" : "nao");
        processed_logged_ = true;
    }

    void render_frame(const FrameTargets& targets) {
        SavedState state = {};
        capture_state(context_, &state);
        const bool gpu_timing_active = gpu_timer_.begin();
        context_->OMSetRenderTargets(0, nullptr, nullptr);

        const FramePlan plan = plan_frame(targets.description);
        log_frame_plan(plan, targets.description);
        capture_scene_for_grade(plan, targets.back_buffer);
        upload_frame_constants(targets, plan);
        bind_common_pipeline_state(targets.description);

        if (plan.bloom) {
            bloom_.render(targets.description, bloom_frame());
        }
        compose_output(targets, plan);

        if (gpu_timing_active) {
            gpu_timer_.end();
        }
        log_first_processed_frame(targets);

        ID3D11ShaderResourceView* null_resources[4] = {};
        context_->PSSetShaderResources(0, 4, null_resources);
        restore_state(context_, &state);
    }

    void render(IDXGISwapChain* swap_chain) {
        if (swap_chain == nullptr || resize_in_progress_) {
            return;
        }

        track_active_swap_chain(swap_chain);
        handle_hotkeys();
        if (!settings_.enabled) {
            return;
        }
        if (!ensure_device_for(swap_chain)) {
            return;
        }

        gpu_timer_.poll();

        FrameTargets targets = {};
        if (acquire_frame_targets(swap_chain, &targets)) {
            render_frame(targets);
        }
        release_frame_targets(&targets);
    }

    void prepare_resize(
        IDXGISwapChain* swap_chain,
        UINT buffer_count,
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        UINT flags) {
        if (swap_chain == nullptr) {
            return;
        }
        if (active_swap_chain_ == nullptr || active_swap_chain_ == swap_chain) {
            resize_in_progress_ = true;
            release_frame_resources();
            active_swap_chain_ = swap_chain;
        }
        log_message(
            "ResizeBuffers solicitado: swap_chain=%p buffers=%u size=%ux%u "
            "format=%u flags=0x%08X.",
            static_cast<void*>(swap_chain),
            buffer_count,
            width,
            height,
            static_cast<unsigned>(format),
            flags);
    }

    void report_resize(IDXGISwapChain* swap_chain, HRESULT result) {
        if (active_swap_chain_ == swap_chain) {
            resize_in_progress_ = false;
        }
        log_message(
            "ResizeBuffers concluido: swap_chain=%p result=0x%08X.",
            static_cast<void*>(swap_chain),
            static_cast<unsigned>(result));
    }

private:

    struct ConstantBufferSlot {
        UINT byte_width;
        ID3D11Buffer* PostProcessor::*member;
        const char* description;
    };

    bool create_constant_buffers() {
        const ConstantBufferSlot slots[] = {
            {sizeof(ShaderConstants), &PostProcessor::constant_buffer_, ""},
            {sizeof(DepthPreviewConstants),
             &PostProcessor::depth_constant_buffer_, " depth"},
            {sizeof(SsaoConstants), &PostProcessor::ssao_constant_buffer_,
             " SSAO"},
            {sizeof(TemporalConstants),
             &PostProcessor::temporal_constant_buffer_, " temporal"},
        };

        for (const ConstantBufferSlot& slot : slots) {
            D3D11_BUFFER_DESC description = {};
            description.ByteWidth = slot.byte_width;
            description.Usage = D3D11_USAGE_DEFAULT;
            description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            const HRESULT result = device_->CreateBuffer(
                &description, nullptr, &(this->*slot.member));
            if (SUCCEEDED(result)) {
                continue;
            }
            log_message(
                "Falha ao criar constant buffer%s: 0x%08X.",
                slot.description,
                static_cast<unsigned>(result));
            return false;
        }
        return true;
    }

    bool create_sampler_states() {
        D3D11_SAMPLER_DESC description = {};
        description.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        description.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        description.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        description.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        description.MaxLOD = FLT_MAX;
        HRESULT result =
            device_->CreateSamplerState(&description, &sampler_state_);
        if (FAILED(result)) {
            log_message(
                "Falha ao criar sampler: 0x%08X.",
                static_cast<unsigned>(result));
            return false;
        }

        description.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        result =
            device_->CreateSamplerState(&description, &depth_sampler_state_);
        if (FAILED(result)) {
            log_message(
                "Falha ao criar sampler depth: 0x%08X.",
                static_cast<unsigned>(result));
            return false;
        }
        return true;
    }

    bool create_raster_states() {
        D3D11_RASTERIZER_DESC rasterizer_description = {};
        rasterizer_description.FillMode = D3D11_FILL_SOLID;
        rasterizer_description.CullMode = D3D11_CULL_NONE;
        rasterizer_description.DepthClipEnable = TRUE;
        HRESULT result = device_->CreateRasterizerState(
            &rasterizer_description, &rasterizer_state_);
        if (FAILED(result)) {
            log_message(
                "Falha ao criar rasterizer state: 0x%08X.",
                static_cast<unsigned>(result));
            return false;
        }

        D3D11_DEPTH_STENCIL_DESC depth_description = {};
        depth_description.DepthEnable = FALSE;
        depth_description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depth_description.DepthFunc = D3D11_COMPARISON_ALWAYS;
        result = device_->CreateDepthStencilState(
            &depth_description, &depth_state_);
        if (FAILED(result)) {
            log_message(
                "Falha ao criar depth state: 0x%08X.",
                static_cast<unsigned>(result));
            return false;
        }
        return true;
    }

    bool create_opaque_blend_state() {
        D3D11_BLEND_DESC description = {};
        description.RenderTarget[0].RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_ALL;
        const HRESULT result =
            device_->CreateBlendState(&description, &blend_state_);
        if (SUCCEEDED(result)) {
            return true;
        }
        log_message(
            "Falha ao criar blend state: 0x%08X.",
            static_cast<unsigned>(result));
        return false;
    }

    void create_additive_blend_state() {
        D3D11_BLEND_DESC description = {};
        description.RenderTarget[0].BlendEnable = TRUE;
        description.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        description.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        description.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        description.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        description.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        description.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        description.RenderTarget[0].RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_ALL;
        const HRESULT result =
            device_->CreateBlendState(&description, &additive_blend_state_);
        if (SUCCEEDED(result)) {
            return;
        }
        log_message(
            "Falha ao criar blend aditivo do bloom: 0x%08X; "
            "o modulo fica indisponivel.",
            static_cast<unsigned>(result));
        safe_release(additive_blend_state_);
    }

    bool recompile_shaders() {
        if (!shaders_.compile(device_)) {
            return false;
        }
        invalidate_temporal_history("recompilacao de shader");
        shaders_.log_state();
        return true;
    }

    bool initialize_pipeline() {
        if (!create_constant_buffers() || !create_sampler_states() ||
            !create_raster_states() || !create_opaque_blend_state()) {
            return false;
        }

        create_additive_blend_state();
        if (!bloom_.attach(device_, context_)) {
            log_message("Falha ao criar constant buffer bloom.");
            return false;
        }
        depth_capture_.attach(device_, context_);
        temporal_.attach(device_, context_);
        gpu_timer_.attach(device_, context_);
        return recompile_shaders();
    }



    void update_condition_adaptation() {
        if (!settings_.condition_adaptation_enabled) {
            effective_temperature_ = settings_.temperature;
            effective_tint_ = settings_.tint;
            return;
        }

        ConditionThresholds thresholds = {};
        thresholds.daylight_median_low = settings_.condition_daylight_median_low;
        thresholds.daylight_median_high =
            settings_.condition_daylight_median_high;
        thresholds.overcast_saturation_low =
            settings_.condition_overcast_saturation_low;
        thresholds.overcast_saturation_high =
            settings_.condition_overcast_saturation_high;
        thresholds.minimum_dynamic_range =
            settings_.condition_minimum_dynamic_range;

        const unsigned long long now = GetTickCount64();
        float elapsed = 0.0f;
        if (condition_last_update_ms_ != 0ull && now > condition_last_update_ms_) {
            elapsed =
                static_cast<float>(now - condition_last_update_ms_) / 1000.0f;
        }
        condition_last_update_ms_ = now;

        if (!condition_smoother_.update(
                scene_observer_.latest(),
                elapsed,
                settings_.condition_time_constant_seconds,
                thresholds)) {
            effective_temperature_ = settings_.temperature;
            effective_tint_ = settings_.tint;
            return;
        }

        ConditionAnchors anchors = {};
        anchors.sun_temperature = settings_.condition_sun_temperature;
        anchors.sun_tint = settings_.condition_sun_tint;
        anchors.rain_temperature = settings_.condition_rain_temperature;
        anchors.rain_tint = settings_.condition_rain_tint;
        anchors.night_temperature = settings_.condition_night_temperature;
        anchors.night_tint = settings_.condition_night_tint;

        const ConditionWeights weights = compute_condition_weights(
            condition_smoother_.median(),
            condition_smoother_.saturation(),
            thresholds);
        const ConditionGrade grade = blend_condition_grade(weights, anchors);
        effective_temperature_ = grade.temperature;
        effective_tint_ = grade.tint;

        const float log_seconds = settings_.condition_log_seconds;
        const bool due =
            log_seconds > 0.0f &&
            now - condition_last_log_ms_ >=
                static_cast<unsigned long long>(log_seconds * 1000.0f);
        if (!condition_logged_once_ || due) {
            log_message(
                "Condicao 0.19.0: sol=%.3f chuva=%.3f noite=%.3f "
                "(mediana=%.1f saturacao=%.3f suavizadas) -> "
                "temperature=%.0fK tint=%.3f.",
                static_cast<double>(weights.sun),
                static_cast<double>(weights.rain),
                static_cast<double>(weights.night),
                static_cast<double>(condition_smoother_.median()),
                static_cast<double>(condition_smoother_.saturation()),
                static_cast<double>(effective_temperature_),
                static_cast<double>(effective_tint_));
            condition_last_log_ms_ = now;
            condition_logged_once_ = true;
        }
    }

    struct IntermediateTarget {
        ID3D11Texture2D* texture;
        ID3D11ShaderResourceView* view;
        ID3D11RenderTargetView* target;
    };

    void release_intermediate_target(IntermediateTarget* intermediate) {
        safe_release(intermediate->target);
        safe_release(intermediate->view);
        safe_release(intermediate->texture);
    }

    D3D11_TEXTURE2D_DESC intermediate_description(
        const D3D11_TEXTURE2D_DESC& source, UINT bind_flags) const {
        D3D11_TEXTURE2D_DESC description = source;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = bind_flags;
        description.CPUAccessFlags = 0;
        description.MiscFlags = 0;
        description.Format = typeless_format(source.Format);
        return description;
    }

    HRESULT create_srgb_view(
        ID3D11Texture2D* texture,
        const D3D11_TEXTURE2D_DESC& source,
        ID3D11ShaderResourceView** view) {
        D3D11_SHADER_RESOURCE_VIEW_DESC description = {};
        description.Format = srgb_view_format(source.Format);
        description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        description.Texture2D.MostDetailedMip = 0;
        description.Texture2D.MipLevels = source.MipLevels;
        return device_->CreateShaderResourceView(texture, &description, view);
    }

    HRESULT create_srgb_target(
        ID3D11Texture2D* texture,
        const D3D11_TEXTURE2D_DESC& source,
        ID3D11RenderTargetView** target) {
        D3D11_RENDER_TARGET_VIEW_DESC description = {};
        description.Format = srgb_view_format(source.Format);
        description.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        description.Texture2D.MipSlice = 0;
        return device_->CreateRenderTargetView(texture, &description, target);
    }

    HRESULT create_intermediate_target(
        const D3D11_TEXTURE2D_DESC& source, IntermediateTarget* intermediate) {
        const D3D11_TEXTURE2D_DESC description = intermediate_description(
            source, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
        HRESULT result = device_->CreateTexture2D(
            &description, nullptr, &intermediate->texture);
        if (FAILED(result) || intermediate->texture == nullptr) {
            return FAILED(result) ? result : E_FAIL;
        }
        result = create_srgb_view(
            intermediate->texture, source, &intermediate->view);
        if (FAILED(result)) {
            return result;
        }
        return create_srgb_target(
            intermediate->texture, source, &intermediate->target);
    }

    bool intermediate_is_complete(
        HRESULT result, const IntermediateTarget& intermediate) {
        return SUCCEEDED(result) && intermediate.texture != nullptr &&
               intermediate.view != nullptr && intermediate.target != nullptr;
    }

    bool create_scene_texture(const D3D11_TEXTURE2D_DESC& source) {
        D3D11_TEXTURE2D_DESC description =
            intermediate_description(source, D3D11_BIND_SHADER_RESOURCE);
        HRESULT result = device_->CreateTexture2D(
            &description, nullptr, &scene_texture_);
        if (SUCCEEDED(result) && scene_texture_ != nullptr) {
            result = create_srgb_view(scene_texture_, source, &scene_view_);
        }

        scene_needs_srgb_decode_ = false;
        if (FAILED(result) || scene_texture_ == nullptr ||
            scene_view_ == nullptr) {
            safe_release(scene_view_);
            safe_release(scene_texture_);

            description.Format = source.Format;
            result = device_->CreateTexture2D(
                &description, nullptr, &scene_texture_);
            if (SUCCEEDED(result) && scene_texture_ != nullptr) {
                result = device_->CreateShaderResourceView(
                    scene_texture_, nullptr, &scene_view_);
            }
            scene_needs_srgb_decode_ = is_unorm_format(source.Format);
            if (!input_fallback_logged_) {
                log_message(
                    "SRV sRGB indisponivel; usando decodificacao manual na entrada.");
                input_fallback_logged_ = true;
            }
        }

        if (SUCCEEDED(result) && scene_texture_ != nullptr &&
            scene_view_ != nullptr) {
            return true;
        }
        log_message(
            "Falha ao criar recursos intermediarios: 0x%08X.",
            static_cast<unsigned>(result));
        safe_release(scene_view_);
        safe_release(scene_texture_);
        return false;
    }

    void create_visual_target(const D3D11_TEXTURE2D_DESC& source) {
        IntermediateTarget created = {};
        const HRESULT result = create_intermediate_target(source, &created);
        if (intermediate_is_complete(result, created)) {
            visual_texture_ = created.texture;
            visual_view_ = created.view;
            visual_target_ = created.target;
            return;
        }
        if (!ssao_resources_failure_logged_) {
            log_message(
                "SSAO 0.9.1 sem textura intermediaria: 0x%08X; "
                "mantendo o passe visual normal.",
                static_cast<unsigned>(result));
            ssao_resources_failure_logged_ = true;
        }
        release_intermediate_target(&created);
    }

    void create_spatial_target(const D3D11_TEXTURE2D_DESC& source) {
        IntermediateTarget created = {};
        const HRESULT result = create_intermediate_target(source, &created);
        if (intermediate_is_complete(result, created)) {
            spatial_texture_ = created.texture;
            spatial_view_ = created.view;
            spatial_target_ = created.target;
            return;
        }
        if (!temporal_.failure_logged()) {
            log_message(
                "Temporal 0.10.0 sem textura espacial: 0x%08X; "
                "mantendo a pilha visual/SSAO anterior.",
                static_cast<unsigned>(result));
            temporal_.set_failure_logged(true);
        }
        release_intermediate_target(&created);
    }

    void release_scene_textures() {
        safe_release(spatial_target_);
        safe_release(spatial_view_);
        safe_release(spatial_texture_);
        safe_release(visual_target_);
        safe_release(visual_view_);
        safe_release(visual_texture_);
        safe_release(scene_view_);
        safe_release(scene_texture_);
    }

    void release_frame_intermediates() {
        release_scene_textures();
        bloom_.release();
        temporal_.release();
        temporal_active_logged_generation_ = 0;
        temporal_wait_logged_ = false;
    }

    bool frame_resources_match(const D3D11_TEXTURE2D_DESC& source) const {
        return scene_texture_ != nullptr && width_ == source.Width &&
               height_ == source.Height && format_ == source.Format;
    }

    bool ensure_frame_resources(const D3D11_TEXTURE2D_DESC& source) {
        if (frame_resources_match(source)) {
            return true;
        }

        release_frame_intermediates();
        if (!create_scene_texture(source)) {
            return false;
        }

        create_visual_target(source);
        if (visual_target_ != nullptr) {
            create_spatial_target(source);
        }

        width_ = source.Width;
        height_ = source.Height;
        format_ = source.Format;
        log_message(
            "Recursos de frame criados: %ux%u format=%u "
            "ssao_intermediate=%s temporal_spatial=%s.",
            width_,
            height_,
            static_cast<unsigned>(format_),
            visual_target_ != nullptr ? "ok" : "indisponivel",
            spatial_target_ != nullptr ? "ok" : "indisponivel");
        return true;
    }







    void invalidate_temporal_history(const char* reason) {
        if (!temporal_.invalidate()) {
            return;
        }
        log_message(
            "Historico temporal 0.10.0 invalidado: %s.",
            reason != nullptr ? reason : "motivo nao informado");
    }


    void release_depth_capture_resources(bool reset_liveness = true) {
        temporal_.release();
        temporal_active_logged_generation_ = 0;
        temporal_wait_logged_ = false;
        depth_capture_.release();
        depth_preview_logged_mode_ = 0;
        depth_preview_wait_logged_ = false;
        ssao_active_logged_generation_ = 0;
        ssao_wait_logged_ = false;
        if (reset_liveness) {
            depth_liveness_generation_ = 0;
            depth_last_binding_serial_ = 0;
            depth_stale_frame_count_ = 0;
            depth_stale_logged_ = false;
        }
    }

    void apply_scene_observer_settings() {
        scene_observer_.configure(
            settings_.scene_observer_enabled,
            static_cast<unsigned>(settings_.scene_observer_interval_frames),
            settings_.scene_observer_log_seconds);

        condition_logged_once_ = false;
        if (!settings_.condition_adaptation_enabled) {
            condition_smoother_.reset();
            effective_temperature_ = settings_.temperature;
            effective_tint_ = settings_.tint;
        }
    }

    void release_frame_resources() {
        release_depth_capture_resources();
        bloom_.release();

        scene_observer_.release();
        release_scene_textures();
        width_ = 0;
        height_ = 0;
        format_ = DXGI_FORMAT_UNKNOWN;
        scene_needs_srgb_decode_ = false;
        unsupported_logged_ = false;
        processed_logged_ = false;
        temporal_.set_failure_logged(false);
        bloom_.set_failure_logged(false);
        bloom_active_logged_levels_ = 0;
    }








    void reset_device() {
        shutdown_steam_screenshots();
        release_frame_resources();
        gpu_timer_.release();
        shaders_.release();
        safe_release(constant_buffer_);
        safe_release(depth_constant_buffer_);
        safe_release(ssao_constant_buffer_);
        safe_release(temporal_constant_buffer_);
        bloom_.release_constant_buffer();
        safe_release(sampler_state_);
        safe_release(depth_sampler_state_);
        safe_release(rasterizer_state_);
        safe_release(depth_state_);
        safe_release(blend_state_);
        safe_release(additive_blend_state_);
        safe_release(context_);
        safe_release(device_);
    }

    Settings settings_;
    SceneObserver scene_observer_;
    GpuTimer gpu_timer_;
    ShaderLibrary shaders_;
    BloomPyramid bloom_;
    TemporalHistory temporal_;
    DepthCapture depth_capture_;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;

    float effective_temperature_ = 6500.0f;
    float effective_tint_ = 0.0f;
    ConditionSmoother condition_smoother_;
    unsigned long long condition_last_update_ms_ = 0ull;
    unsigned long long condition_last_log_ms_ = 0ull;
    bool condition_logged_once_ = false;

    ID3D11Texture2D* scene_texture_ = nullptr;
    ID3D11ShaderResourceView* scene_view_ = nullptr;
    ID3D11Texture2D* visual_texture_ = nullptr;
    ID3D11ShaderResourceView* visual_view_ = nullptr;
    ID3D11RenderTargetView* visual_target_ = nullptr;
    ID3D11Texture2D* spatial_texture_ = nullptr;
    ID3D11ShaderResourceView* spatial_view_ = nullptr;
    ID3D11RenderTargetView* spatial_target_ = nullptr;

    ID3D11Buffer* constant_buffer_ = nullptr;
    ID3D11Buffer* depth_constant_buffer_ = nullptr;
    ID3D11Buffer* ssao_constant_buffer_ = nullptr;
    ID3D11Buffer* temporal_constant_buffer_ = nullptr;
    ID3D11SamplerState* sampler_state_ = nullptr;
    ID3D11SamplerState* depth_sampler_state_ = nullptr;
    ID3D11RasterizerState* rasterizer_state_ = nullptr;
    ID3D11DepthStencilState* depth_state_ = nullptr;
    ID3D11BlendState* blend_state_ = nullptr;
    ID3D11BlendState* additive_blend_state_ = nullptr;
    IDXGISwapChain* active_swap_chain_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    bool unsupported_logged_ = false;
    bool processed_logged_ = false;
    bool scene_needs_srgb_decode_ = false;
    bool input_fallback_logged_ = false;
    bool output_fallback_logged_ = false;
    bool ssao_resources_failure_logged_ = false;
    bool bloom_wait_logged_ = false;
    bool resize_in_progress_ = false;
    bool home_key_down_ = false;
    bool end_key_down_ = false;
    bool insert_key_down_ = false;
    UINT depth_preview_mode_ = 0;
    UINT depth_preview_logged_mode_ = 0;
    bool depth_preview_wait_logged_ = false;
    bool ssao_wait_logged_ = false;
    bool temporal_wait_logged_ = false;
    std::uint64_t ssao_active_logged_generation_ = 0;
    std::uint64_t temporal_active_logged_generation_ = 0;
    UINT bloom_active_logged_levels_ = 0;
    std::uint64_t depth_liveness_generation_ = 0;
    std::uint64_t depth_last_binding_serial_ = 0;
    UINT depth_stale_frame_count_ = 0;
    bool depth_stale_logged_ = false;
    static constexpr UINT kDepthActivityGraceFrames = 2;
    static constexpr UINT kDepthStaleFrameThreshold = 30;
};

PostProcessor g_post_processor;
thread_local bool g_inside_present = false;
SRWLOCK g_processor_lock = SRWLOCK_INIT;
}

void process_frame(IDXGISwapChain* swap_chain) {
    if (g_inside_present) {
        return;
    }
    g_inside_present = true;
    AcquireSRWLockExclusive(&g_processor_lock);
    g_post_processor.render(swap_chain);
    ReleaseSRWLockExclusive(&g_processor_lock);
    g_inside_present = false;
}

bool is_processing_frame() {
    return g_inside_present;
}

void prepare_for_resize(
    IDXGISwapChain* swap_chain,
    UINT buffer_count,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT flags) {
    AcquireSRWLockExclusive(&g_processor_lock);
    g_post_processor.prepare_resize(
        swap_chain, buffer_count, width, height, format, flags);
    ReleaseSRWLockExclusive(&g_processor_lock);
}

void report_resize_result(IDXGISwapChain* swap_chain, HRESULT result) {
    AcquireSRWLockExclusive(&g_processor_lock);
    g_post_processor.report_resize(swap_chain, result);
    ReleaseSRWLockExclusive(&g_processor_lock);
}
}
