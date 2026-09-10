#include "postprocess.hpp"

#include "config.hpp"
#include "resource_observer.hpp"
#include "runtime.hpp"
#include "scene_observer.hpp"
#include "scene_conditions.hpp"
#include "steam_screenshots.hpp"

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
using CompileFromFileFunction = decltype(&D3DCompileFromFile);

CompileFromFileFunction resolve_shader_compiler() {
    static CompileFromFileFunction function = []() -> CompileFromFileFunction {
        HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
        if (compiler == nullptr) {
            log_message(
                "Nao foi possivel carregar d3dcompiler_47.dll: %lu.",
                GetLastError());
            return nullptr;
        }
        return reinterpret_cast<CompileFromFileFunction>(
            GetProcAddress(compiler, "D3DCompileFromFile"));
    }();
    return function;
}

template <typename T>
void safe_release(T*& object) {
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

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

struct ShaderConstants {
    float texel_size[2];
    float exposure;
    float temperature;
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
    float input_needs_srgb_decode;
    float output_needs_srgb_encode;

    float black_lift[3];
    float highlight_rolloff;
    float tint;
    float bloom_enabled;
    float bloom_intensity;
    float bloom_padding;
};

static_assert(sizeof(ShaderConstants) == 96, "constant buffer must be aligned");

struct BloomConstants {
    float source_texel_size[2];
    float filter_radius[2];
    float threshold;
    float knee;
    float input_needs_srgb_decode;
    float output_needs_srgb_encode;
};

static_assert(
    sizeof(BloomConstants) == 32,
    "bloom constant buffer must be aligned");

constexpr UINT kBloomPassCount = 3;

const char* const kBloomEntryPoints[kBloomPassCount] = {
    "PSBloomBright",
    "PSBloomDownsample",
    "PSBloomUpsample",
};

struct DepthPreviewConstants {
    float preview_mode;
    float output_needs_srgb_encode;
    float near_plane;
    float preview_distance;
    float texel_size[2];
    float projection_scale[2];
};

static_assert(
    sizeof(DepthPreviewConstants) == 32,
    "depth preview constant buffer must be aligned");

struct SsaoConstants {
    float input_needs_srgb_decode;
    float output_needs_srgb_encode;
    float near_plane;
    float radius;
    float intensity;
    float bias;
    float fade_start;
    float fade_end;
    float edge_rejection;
    float debug_mode;
    float depth_texel_size[2];
    float projection_scale[2];
    float padding[2];
    float refinement_enabled;
    float highlight_start;
    float highlight_end;
    float highlight_ao_floor;
    float interior_enabled;
    float interior_near_start;
    float interior_near_end;
    float interior_radius;
    float interior_intensity;
    float interior_bias;
    float interior_edge_rejection;
    float interior_padding;
};

static_assert(
    sizeof(SsaoConstants) == 112,
    "SSAO constant buffer must be aligned");

struct TemporalConstants {
    float texel_size[2];
    float near_plane;
    float history_weight;
    float depth_rejection;
    float color_rejection;
    float history_valid;
    float output_needs_srgb_encode;
};

static_assert(
    sizeof(TemporalConstants) == 32,
    "temporal constant buffer must be aligned");

struct SavedState {
    ID3D11RenderTargetView* render_targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* depth_target = nullptr;
    ID3D11BlendState* blend_state = nullptr;
    FLOAT blend_factor[4] = {};
    UINT sample_mask = 0;
    ID3D11DepthStencilState* depth_state = nullptr;
    UINT stencil_reference = 0;
    ID3D11RasterizerState* rasterizer_state = nullptr;
    D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    UINT viewport_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_RECT scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    UINT scissor_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    ID3D11InputLayout* input_layout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11VertexShader* vertex_shader = nullptr;
    ID3D11PixelShader* pixel_shader = nullptr;
    ID3D11GeometryShader* geometry_shader = nullptr;
    ID3D11HullShader* hull_shader = nullptr;
    ID3D11DomainShader* domain_shader = nullptr;
    ID3D11ShaderResourceView* pixel_resources[4] = {};
    ID3D11SamplerState* pixel_samplers[2] = {};
    ID3D11Buffer* pixel_constant_buffer = nullptr;
};

struct GpuTimingSlot {
    ID3D11Query* disjoint = nullptr;
    ID3D11Query* start = nullptr;
    ID3D11Query* end = nullptr;
    bool pending = false;
};

void capture_state(ID3D11DeviceContext* context, SavedState* state) {
    context->OMGetRenderTargets(
        D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
        state->render_targets,
        &state->depth_target);
    context->OMGetBlendState(
        &state->blend_state, state->blend_factor, &state->sample_mask);
    context->OMGetDepthStencilState(
        &state->depth_state, &state->stencil_reference);
    context->RSGetState(&state->rasterizer_state);
    context->RSGetViewports(&state->viewport_count, state->viewports);
    context->RSGetScissorRects(&state->scissor_count, state->scissors);
    context->IAGetInputLayout(&state->input_layout);
    context->IAGetPrimitiveTopology(&state->topology);
    context->VSGetShader(&state->vertex_shader, nullptr, nullptr);
    context->PSGetShader(&state->pixel_shader, nullptr, nullptr);
    context->GSGetShader(&state->geometry_shader, nullptr, nullptr);
    context->HSGetShader(&state->hull_shader, nullptr, nullptr);
    context->DSGetShader(&state->domain_shader, nullptr, nullptr);
    context->PSGetShaderResources(0, 4, state->pixel_resources);
    context->PSGetSamplers(0, 2, state->pixel_samplers);
    context->PSGetConstantBuffers(0, 1, &state->pixel_constant_buffer);
}

void restore_state(ID3D11DeviceContext* context, SavedState* state) {
    context->OMSetRenderTargets(
        D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
        state->render_targets,
        state->depth_target);
    context->OMSetBlendState(
        state->blend_state, state->blend_factor, state->sample_mask);
    context->OMSetDepthStencilState(
        state->depth_state, state->stencil_reference);
    context->RSSetState(state->rasterizer_state);
    context->RSSetViewports(state->viewport_count, state->viewports);
    context->RSSetScissorRects(state->scissor_count, state->scissors);
    context->IASetInputLayout(state->input_layout);
    context->IASetPrimitiveTopology(state->topology);
    context->VSSetShader(state->vertex_shader, nullptr, 0);
    context->PSSetShader(state->pixel_shader, nullptr, 0);
    context->GSSetShader(state->geometry_shader, nullptr, 0);
    context->HSSetShader(state->hull_shader, nullptr, 0);
    context->DSSetShader(state->domain_shader, nullptr, 0);
    context->PSSetShaderResources(0, 4, state->pixel_resources);
    context->PSSetSamplers(0, 2, state->pixel_samplers);
    context->PSSetConstantBuffers(0, 1, &state->pixel_constant_buffer);

    for (ID3D11RenderTargetView*& target : state->render_targets) {
        safe_release(target);
    }
    safe_release(state->depth_target);
    safe_release(state->blend_state);
    safe_release(state->depth_state);
    safe_release(state->rasterizer_state);
    safe_release(state->input_layout);
    safe_release(state->vertex_shader);
    safe_release(state->pixel_shader);
    safe_release(state->geometry_shader);
    safe_release(state->hull_shader);
    safe_release(state->domain_shader);
    for (ID3D11ShaderResourceView*& resource : state->pixel_resources) {
        safe_release(resource);
    }
    for (ID3D11SamplerState*& sampler : state->pixel_samplers) {
        safe_release(sampler);
    }
    safe_release(state->pixel_constant_buffer);
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
            compile_shaders();
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
            safe && ensure_depth_capture_resources(
                        candidate, depth.description, depth.generation);
        if (depth.available) {
            context_->CopyResource(depth_copy_texture_, candidate);
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
            settings_.temporal_enabled && temporal_shader_ != nullptr &&
            visual_target_ != nullptr && visual_view_ != nullptr;
        if (!eligible) {
            return false;
        }
        if (!ensure_temporal_resources(
                description, depth.description, depth.generation)) {
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
            settings_.bloom_enabled && bloom_bright_shader_ != nullptr &&
            additive_blend_state_ != nullptr;
        if (!eligible) {
            return false;
        }
        return ensure_bloom_resources(
            description,
            bloom_levels_for_radius(
                settings_.bloom_radius, description.Height));
    }

    FramePlan plan_frame(const D3D11_TEXTURE2D_DESC& description) {
        FramePlan plan = {};
        plan.depth = acquire_depth_state();

        plan.depth_preview =
            plan.depth.available && depth_preview_mode_ >= 1 &&
            depth_preview_mode_ <= 4 && depth_preview_shader_ != nullptr;
        plan.ssao_preview =
            plan.depth.available && depth_preview_mode_ == 5 &&
            ssao_shader_ != nullptr;
        plan.ssao =
            plan.depth.available && depth_preview_mode_ == 0 &&
            settings_.ssao_enabled && ssao_shader_ != nullptr &&
            visual_target_ != nullptr && visual_view_ != nullptr;
        plan.temporal = plan_temporal(description, plan.depth, plan.ssao);

        if (!plan.temporal && temporal_history_valid_) {
            invalidate_temporal_history("depth ou passe temporal indisponivel");
        }

        plan.bloom_preview =
            depth_preview_mode_ == 6 && bloom_bright_shader_ != nullptr &&
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
            if (bloom_active_logged_levels_ != bloom_level_count_) {
                log_message(
                    "Bloom 0.17.0 ativo: niveis=%u threshold=%.3f knee=%.3f "
                    "intensity=%.3f radius=%.4f.",
                    bloom_level_count_,
                    settings_.bloom_threshold,
                    settings_.bloom_knee,
                    settings_.bloom_intensity,
                    settings_.bloom_radius);
                bloom_active_logged_levels_ = bloom_level_count_;
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
            temporal_history_valid_ ? 1.0f : 0.0f;
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
        context_->VSSetShader(vertex_shader_, nullptr, 0);
        context_->GSSetShader(nullptr, nullptr, 0);
        context_->HSSetShader(nullptr, nullptr, 0);
        context_->DSSetShader(nullptr, nullptr, 0);
    }

    void draw_visual_pass(
        ID3D11RenderTargetView* target, const FramePlan& plan) {
        context_->OMSetRenderTargets(1, &target, nullptr);
        context_->PSSetShader(pixel_shader_, nullptr, 0);
        ID3D11ShaderResourceView* visual_resources[2] = {
            scene_view_, plan.bloom ? bloom_views_[0] : nullptr};
        context_->PSSetShaderResources(0, 2, visual_resources);
        context_->PSSetSamplers(0, 1, &sampler_state_);
        context_->PSSetConstantBuffers(0, 1, &constant_buffer_);
        context_->Draw(3, 0);
    }

    void draw_ssao_pass(
        ID3D11RenderTargetView* target, ID3D11ShaderResourceView* source) {
        context_->OMSetRenderTargets(1, &target, nullptr);
        context_->PSSetShader(ssao_shader_, nullptr, 0);
        ID3D11ShaderResourceView* ssao_resources[2] = {
            source, depth_copy_view_};
        ID3D11SamplerState* ssao_samplers[2] = {
            sampler_state_, depth_sampler_state_};
        context_->PSSetShaderResources(0, 2, ssao_resources);
        context_->PSSetSamplers(0, 2, ssao_samplers);
        context_->PSSetConstantBuffers(0, 1, &ssao_constant_buffer_);
        context_->Draw(3, 0);
    }

    void draw_bloom_preview(const FrameTargets& targets) {
        context_->OMSetRenderTargets(1, &targets.output, nullptr);
        context_->PSSetShader(bloom_upsample_shader_, nullptr, 0);
        BloomConstants preview_constants = {};
        preview_constants.source_texel_size[0] =
            1.0f / static_cast<float>(bloom_widths_[0]);
        preview_constants.source_texel_size[1] =
            1.0f / static_cast<float>(bloom_heights_[0]);
        preview_constants.threshold = settings_.bloom_threshold;
        preview_constants.knee = settings_.bloom_knee;
        preview_constants.output_needs_srgb_encode =
            targets.output_needs_srgb_encode ? 1.0f : 0.0f;
        context_->UpdateSubresource(
            bloom_constant_buffer_, 0, nullptr, &preview_constants, 0, 0);
        context_->PSSetShaderResources(0, 1, &bloom_views_[0]);
        context_->PSSetSamplers(0, 1, &sampler_state_);
        context_->PSSetConstantBuffers(0, 1, &bloom_constant_buffer_);
        context_->Draw(3, 0);
    }

    void draw_depth_preview(const FrameTargets& targets) {
        context_->OMSetRenderTargets(1, &targets.output, nullptr);
        context_->PSSetShader(depth_preview_shader_, nullptr, 0);
        context_->PSSetShaderResources(0, 1, &depth_copy_view_);
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
        context_->PSSetShader(temporal_shader_, nullptr, 0);
        ID3D11ShaderResourceView* temporal_resources[4] = {
            current_view,
            temporal_history_valid_ ? temporal_history_view_ : nullptr,
            depth_copy_view_,
            temporal_history_valid_ ? temporal_depth_history_view_ : nullptr};
        ID3D11SamplerState* temporal_samplers[2] = {
            sampler_state_, depth_sampler_state_};
        context_->PSSetShaderResources(0, 4, temporal_resources);
        context_->PSSetSamplers(0, 2, temporal_samplers);
        context_->PSSetConstantBuffers(0, 1, &temporal_constant_buffer_);
        context_->Draw(3, 0);

        context_->OMSetRenderTargets(0, nullptr, nullptr);
        ID3D11ShaderResourceView* null_temporal_resources[4] = {};
        context_->PSSetShaderResources(0, 4, null_temporal_resources);
        context_->CopyResource(
            temporal_history_texture_, targets.back_buffer);
        context_->CopyResource(
            temporal_depth_history_texture_, depth_copy_texture_);
        if (temporal_history_valid_) {
            return;
        }
        temporal_history_valid_ = true;
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
            draw_bloom_preview(targets);
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
        const bool gpu_timing_active = begin_gpu_timing();
        context_->OMSetRenderTargets(0, nullptr, nullptr);

        const FramePlan plan = plan_frame(targets.description);
        log_frame_plan(plan, targets.description);
        capture_scene_for_grade(plan, targets.back_buffer);
        upload_frame_constants(targets, plan);
        bind_common_pipeline_state(targets.description);

        if (plan.bloom) {
            render_bloom_pyramid(targets.description);
        }
        compose_output(targets, plan);

        if (gpu_timing_active) {
            end_gpu_timing();
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

        poll_gpu_timing();

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
    bool is_unorm_format(DXGI_FORMAT format) const {
        return format == DXGI_FORMAT_B8G8R8A8_UNORM ||
               format == DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    DXGI_FORMAT typeless_format(DXGI_FORMAT format) const {
        if (format == DXGI_FORMAT_B8G8R8A8_UNORM ||
            format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
            return DXGI_FORMAT_B8G8R8A8_TYPELESS;
        }
        return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    }

    DXGI_FORMAT srgb_view_format(DXGI_FORMAT format) const {
        if (format == DXGI_FORMAT_B8G8R8A8_UNORM ||
            format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        }
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }

    bool is_supported_format(DXGI_FORMAT format) const {
        return format == DXGI_FORMAT_B8G8R8A8_UNORM ||
               format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
               format == DXGI_FORMAT_R8G8B8A8_UNORM ||
               format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }

    bool depth_copy_formats(
        DXGI_FORMAT source,
        DXGI_FORMAT* resource_format,
        DXGI_FORMAT* view_format) const {
        if (resource_format == nullptr || view_format == nullptr) {
            return false;
        }

        if (source == DXGI_FORMAT_D32_FLOAT_S8X24_UINT ||
            source == DXGI_FORMAT_R32G8X24_TYPELESS) {
            *resource_format = DXGI_FORMAT_R32G8X24_TYPELESS;
            *view_format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
            return true;
        }
        if (source == DXGI_FORMAT_D32_FLOAT ||
            source == DXGI_FORMAT_R32_TYPELESS) {
            *resource_format = DXGI_FORMAT_R32_TYPELESS;
            *view_format = DXGI_FORMAT_R32_FLOAT;
            return true;
        }
        if (source == DXGI_FORMAT_D24_UNORM_S8_UINT ||
            source == DXGI_FORMAT_R24G8_TYPELESS) {
            *resource_format = DXGI_FORMAT_R24G8_TYPELESS;
            *view_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            return true;
        }
        if (source == DXGI_FORMAT_D16_UNORM ||
            source == DXGI_FORMAT_R16_TYPELESS) {
            *resource_format = DXGI_FORMAT_R16_TYPELESS;
            *view_format = DXGI_FORMAT_R16_UNORM;
            return true;
        }
        return false;
    }

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
            {sizeof(BloomConstants), &PostProcessor::bloom_constant_buffer_,
             " bloom"},
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

    bool initialize_pipeline() {
        if (!create_constant_buffers() || !create_sampler_states() ||
            !create_raster_states() || !create_opaque_blend_state()) {
            return false;
        }

        create_additive_blend_state();
        initialize_gpu_timing();
        return compile_shaders();
    }

    ID3DBlob* compile_shader_blob(
        CompileFromFileFunction compile_from_file,
        const wchar_t* path,
        const char* entry_point,
        const char* target,
        const char* stage) {
        constexpr UINT kFlags =
            D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
        ID3DBlob* blob = nullptr;
        ID3DBlob* errors = nullptr;
        const HRESULT result = compile_from_file(
            path,
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entry_point,
            target,
            kFlags,
            0,
            &blob,
            &errors);
        if (FAILED(result)) {
            log_compile_error(stage, result, errors);
            safe_release(blob);
        }
        safe_release(errors);
        return blob;
    }

    ID3D11PixelShader* create_optional_pixel_shader(
        ID3DBlob* blob, const char* description) {
        if (blob == nullptr) {
            return nullptr;
        }
        ID3D11PixelShader* shader = nullptr;
        const HRESULT result = device_->CreatePixelShader(
            blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader);
        if (SUCCEEDED(result)) {
            return shader;
        }
        log_message(
            "Falha ao criar shader %s: 0x%08X.",
            description,
            static_cast<unsigned>(result));
        safe_release(shader);
        return nullptr;
    }

    void adopt_optional_shader(
        ID3D11PixelShader** slot, ID3D11PixelShader* fresh) {
        if (fresh == nullptr) {
            return;
        }
        safe_release(*slot);
        *slot = fresh;
    }

    struct ShaderBlobs {
        ID3DBlob* vertex;
        ID3DBlob* pixel;
        ID3DBlob* depth_preview;
        ID3DBlob* ssao;
        ID3DBlob* temporal;
        ID3DBlob* bloom[kBloomPassCount];
    };

    void release_shader_blobs(ShaderBlobs* blobs) {
        safe_release(blobs->vertex);
        safe_release(blobs->pixel);
        safe_release(blobs->depth_preview);
        safe_release(blobs->ssao);
        safe_release(blobs->temporal);
        for (UINT index = 0; index < kBloomPassCount; ++index) {
            safe_release(blobs->bloom[index]);
        }
    }

    bool compile_shader_blobs(
        CompileFromFileFunction compile_from_file, ShaderBlobs* blobs) {
        blobs->vertex = compile_shader_blob(
            compile_from_file, shader_path(), "VSMain", "vs_5_0", "vertex");
        if (blobs->vertex == nullptr) {
            return false;
        }
        blobs->pixel = compile_shader_blob(
            compile_from_file, shader_path(), "PSMain", "ps_5_0", "pixel");
        if (blobs->pixel == nullptr) {
            return false;
        }

        blobs->depth_preview = compile_shader_blob(
            compile_from_file,
            depth_preview_shader_path(),
            "PSDepthPreview",
            "ps_5_0",
            "depth preview");
        blobs->ssao = compile_shader_blob(
            compile_from_file, ssao_shader_path(), "PSSSAO", "ps_5_0", "SSAO");
        blobs->temporal = compile_shader_blob(
            compile_from_file,
            temporal_shader_path(),
            "PSTemporal",
            "ps_5_0",
            "temporal");
        for (UINT index = 0; index < kBloomPassCount; ++index) {
            blobs->bloom[index] = compile_shader_blob(
                compile_from_file,
                bloom_shader_path(),
                kBloomEntryPoints[index],
                "ps_5_0",
                kBloomEntryPoints[index]);
        }
        return true;
    }

    struct CompiledShaders {
        ID3D11VertexShader* vertex;
        ID3D11PixelShader* pixel;
        ID3D11PixelShader* depth_preview;
        ID3D11PixelShader* ssao;
        ID3D11PixelShader* temporal;
        ID3D11PixelShader* bloom[kBloomPassCount];
    };

    void release_compiled_shaders(CompiledShaders* shaders) {
        safe_release(shaders->vertex);
        safe_release(shaders->pixel);
        safe_release(shaders->depth_preview);
        safe_release(shaders->ssao);
        safe_release(shaders->temporal);
        for (UINT index = 0; index < kBloomPassCount; ++index) {
            safe_release(shaders->bloom[index]);
        }
    }

    HRESULT create_core_shaders(
        const ShaderBlobs& blobs, CompiledShaders* shaders) {
        HRESULT result = device_->CreateVertexShader(
            blobs.vertex->GetBufferPointer(),
            blobs.vertex->GetBufferSize(),
            nullptr,
            &shaders->vertex);
        if (FAILED(result)) {
            return result;
        }
        return device_->CreatePixelShader(
            blobs.pixel->GetBufferPointer(),
            blobs.pixel->GetBufferSize(),
            nullptr,
            &shaders->pixel);
    }

    void create_optional_shaders(
        const ShaderBlobs& blobs, CompiledShaders* shaders) {
        shaders->depth_preview =
            create_optional_pixel_shader(blobs.depth_preview, "de preview depth");
        shaders->ssao = create_optional_pixel_shader(blobs.ssao, "SSAO");
        shaders->temporal =
            create_optional_pixel_shader(blobs.temporal, "temporal");
        for (UINT index = 0; index < kBloomPassCount; ++index) {
            char description[64] = {};
            std::snprintf(
                description,
                sizeof(description),
                "%s do bloom",
                kBloomEntryPoints[index]);
            shaders->bloom[index] =
                create_optional_pixel_shader(blobs.bloom[index], description);
        }
    }

    void adopt_bloom_shaders(CompiledShaders* shaders) {
        bool complete = true;
        for (UINT index = 0; index < kBloomPassCount; ++index) {
            complete = complete && shaders->bloom[index] != nullptr;
        }
        if (!complete) {
            for (UINT index = 0; index < kBloomPassCount; ++index) {
                safe_release(shaders->bloom[index]);
            }
            return;
        }
        safe_release(bloom_bright_shader_);
        safe_release(bloom_downsample_shader_);
        safe_release(bloom_upsample_shader_);
        bloom_bright_shader_ = shaders->bloom[0];
        bloom_downsample_shader_ = shaders->bloom[1];
        bloom_upsample_shader_ = shaders->bloom[2];
    }

    void adopt_compiled_shaders(CompiledShaders* shaders) {
        safe_release(vertex_shader_);
        safe_release(pixel_shader_);
        vertex_shader_ = shaders->vertex;
        pixel_shader_ = shaders->pixel;
        adopt_optional_shader(&depth_preview_shader_, shaders->depth_preview);
        adopt_optional_shader(&ssao_shader_, shaders->ssao);
        adopt_optional_shader(&temporal_shader_, shaders->temporal);
        adopt_bloom_shaders(shaders);
    }

    bool compile_shaders() {
        const CompileFromFileFunction compile_from_file =
            resolve_shader_compiler();
        if (compile_from_file == nullptr) {
            log_message("D3DCompileFromFile nao esta disponivel.");
            return false;
        }

        ShaderBlobs blobs = {};
        if (!compile_shader_blobs(compile_from_file, &blobs)) {
            release_shader_blobs(&blobs);
            return false;
        }

        CompiledShaders shaders = {};
        const HRESULT result = create_core_shaders(blobs, &shaders);
        create_optional_shaders(blobs, &shaders);
        release_shader_blobs(&blobs);

        if (FAILED(result)) {
            log_message(
                "Falha ao criar shaders D3D11: 0x%08X.",
                static_cast<unsigned>(result));
            release_compiled_shaders(&shaders);
            return false;
        }

        adopt_compiled_shaders(&shaders);
        invalidate_temporal_history("recompilacao de shader");
        log_message(
            "Shaders Photorealism compilados: visual=ok depth_preview=%s "
            "ssao_0.9.1=%s temporal_0.10.0=%s bloom_0.17.0=%s.",
            depth_preview_shader_ != nullptr ? "ok" : "indisponivel",
            ssao_shader_ != nullptr ? "ok" : "indisponivel",
            temporal_shader_ != nullptr ? "ok" : "indisponivel",
            bloom_bright_shader_ != nullptr ? "ok" : "indisponivel");
        return true;
    }

    void log_compile_error(const char* stage, HRESULT result, ID3DBlob* errors) {
        if (errors != nullptr && errors->GetBufferPointer() != nullptr) {
            log_message(
                "Erro no shader %s (0x%08X): %s",
                stage,
                static_cast<unsigned>(result),
                static_cast<const char*>(errors->GetBufferPointer()));
        } else {
            log_message(
                "Erro no shader %s: 0x%08X.",
                stage,
                static_cast<unsigned>(result));
        }
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
        if (!temporal_resources_failure_logged_) {
            log_message(
                "Temporal 0.10.0 sem textura espacial: 0x%08X; "
                "mantendo a pilha visual/SSAO anterior.",
                static_cast<unsigned>(result));
            temporal_resources_failure_logged_ = true;
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
        release_bloom_resources();
        release_temporal_resources();
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

    UINT bloom_levels_for_radius(float radius, UINT height) const {
        if (radius <= 0.0f || height == 0) {
            return 1;
        }
        const float target = radius * static_cast<float>(height) / 1.5f;
        UINT levels = 1;
        while (levels < kBloomMaxLevels &&
               static_cast<float>(1u << levels) < target) {
            ++levels;
        }
        return levels;
    }

    void release_bloom_resources() {
        for (UINT index = 0; index < kBloomMaxLevels; ++index) {
            safe_release(bloom_targets_[index]);
            safe_release(bloom_views_[index]);
            safe_release(bloom_textures_[index]);
            bloom_widths_[index] = 0;
            bloom_heights_[index] = 0;
        }
        bloom_level_count_ = 0;
    }

    bool ensure_bloom_resources(
        const D3D11_TEXTURE2D_DESC& source, UINT requested_levels) {
        if (bloom_level_count_ == requested_levels &&
            bloom_textures_[0] != nullptr &&
            bloom_widths_[0] == std::max(source.Width >> 1, 1u) &&
            bloom_heights_[0] == std::max(source.Height >> 1, 1u)) {
            return true;
        }

        release_bloom_resources();

        HRESULT result = S_OK;
        for (UINT index = 0; index < requested_levels; ++index) {
            const UINT level_width =
                std::max(source.Width >> (index + 1), 1u);
            const UINT level_height =
                std::max(source.Height >> (index + 1), 1u);

            if (level_width < 8 || level_height < 8) {
                break;
            }

            D3D11_TEXTURE2D_DESC description = source;
            description.Width = level_width;
            description.Height = level_height;
            description.MipLevels = 1;
            description.ArraySize = 1;
            description.SampleDesc.Count = 1;
            description.SampleDesc.Quality = 0;
            description.Usage = D3D11_USAGE_DEFAULT;
            description.BindFlags =
                D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            description.CPUAccessFlags = 0;
            description.MiscFlags = 0;
            description.Format = typeless_format(source.Format);

            result = device_->CreateTexture2D(
                &description, nullptr, &bloom_textures_[index]);
            if (SUCCEEDED(result)) {
                D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
                view_description.Format = srgb_view_format(source.Format);
                view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                view_description.Texture2D.MostDetailedMip = 0;
                view_description.Texture2D.MipLevels = 1;
                result = device_->CreateShaderResourceView(
                    bloom_textures_[index],
                    &view_description,
                    &bloom_views_[index]);
            }
            if (SUCCEEDED(result)) {
                D3D11_RENDER_TARGET_VIEW_DESC target_description = {};
                target_description.Format = srgb_view_format(source.Format);
                target_description.ViewDimension =
                    D3D11_RTV_DIMENSION_TEXTURE2D;
                target_description.Texture2D.MipSlice = 0;
                result = device_->CreateRenderTargetView(
                    bloom_textures_[index],
                    &target_description,
                    &bloom_targets_[index]);
            }
            if (FAILED(result)) {
                break;
            }

            bloom_widths_[index] = level_width;
            bloom_heights_[index] = level_height;
            ++bloom_level_count_;
        }

        if (bloom_level_count_ < 2) {
            if (!bloom_resources_failure_logged_) {
                log_message(
                    "Bloom 0.17.0 sem piramide utilizavel (niveis=%u, "
                    "0x%08X); mantendo a pilha visual aprovada.",
                    bloom_level_count_,
                    static_cast<unsigned>(result));
                bloom_resources_failure_logged_ = true;
            }
            release_bloom_resources();
            return false;
        }

        log_message(
            "Bloom 0.17.0 com piramide de %u niveis: %ux%u ate %ux%u.",
            bloom_level_count_,
            bloom_widths_[0],
            bloom_heights_[0],
            bloom_widths_[bloom_level_count_ - 1],
            bloom_heights_[bloom_level_count_ - 1]);
        return true;
    }

    void render_bloom_pyramid(const D3D11_TEXTURE2D_DESC& description) {
        BloomConstants constants = {};
        constants.threshold = settings_.bloom_threshold;
        constants.knee = settings_.bloom_knee;
        constants.input_needs_srgb_decode =
            scene_needs_srgb_decode_ ? 1.0f : 0.0f;
        constants.filter_radius[0] = 1.0f;
        constants.filter_radius[1] = 1.0f;

        D3D11_VIEWPORT viewport = {};
        viewport.MaxDepth = 1.0f;

        for (UINT index = 0; index < bloom_level_count_; ++index) {
            const UINT source_width =
                index == 0 ? description.Width : bloom_widths_[index - 1];
            const UINT source_height =
                index == 0 ? description.Height : bloom_heights_[index - 1];
            constants.source_texel_size[0] =
                1.0f / static_cast<float>(source_width);
            constants.source_texel_size[1] =
                1.0f / static_cast<float>(source_height);
            context_->UpdateSubresource(
                bloom_constant_buffer_, 0, nullptr, &constants, 0, 0);

            viewport.Width = static_cast<float>(bloom_widths_[index]);
            viewport.Height = static_cast<float>(bloom_heights_[index]);
            context_->RSSetViewports(1, &viewport);
            context_->OMSetRenderTargets(1, &bloom_targets_[index], nullptr);
            context_->PSSetShader(
                index == 0 ? bloom_bright_shader_ : bloom_downsample_shader_,
                nullptr,
                0);
            ID3D11ShaderResourceView* source_view =
                index == 0 ? scene_view_ : bloom_views_[index - 1];
            context_->PSSetShaderResources(0, 1, &source_view);
            context_->PSSetSamplers(0, 1, &sampler_state_);
            context_->PSSetConstantBuffers(0, 1, &bloom_constant_buffer_);
            context_->Draw(3, 0);
            context_->OMSetRenderTargets(0, nullptr, nullptr);
        }

        context_->OMSetBlendState(
            additive_blend_state_, nullptr, 0xFFFFFFFFu);
        for (UINT index = bloom_level_count_ - 1; index > 0; --index) {
            constants.source_texel_size[0] =
                1.0f / static_cast<float>(bloom_widths_[index]);
            constants.source_texel_size[1] =
                1.0f / static_cast<float>(bloom_heights_[index]);
            context_->UpdateSubresource(
                bloom_constant_buffer_, 0, nullptr, &constants, 0, 0);

            viewport.Width = static_cast<float>(bloom_widths_[index - 1]);
            viewport.Height = static_cast<float>(bloom_heights_[index - 1]);
            context_->RSSetViewports(1, &viewport);
            context_->OMSetRenderTargets(
                1, &bloom_targets_[index - 1], nullptr);
            context_->PSSetShader(bloom_upsample_shader_, nullptr, 0);
            context_->PSSetShaderResources(0, 1, &bloom_views_[index]);
            context_->PSSetSamplers(0, 1, &sampler_state_);
            context_->PSSetConstantBuffers(0, 1, &bloom_constant_buffer_);
            context_->Draw(3, 0);
            context_->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11ShaderResourceView* null_view = nullptr;
            context_->PSSetShaderResources(0, 1, &null_view);
        }
        context_->OMSetBlendState(blend_state_, nullptr, 0xFFFFFFFFu);

        viewport.Width = static_cast<float>(description.Width);
        viewport.Height = static_cast<float>(description.Height);
        context_->RSSetViewports(1, &viewport);
    }

    bool ensure_depth_capture_resources(
        ID3D11Texture2D* source,
        const D3D11_TEXTURE2D_DESC& source_description,
        std::uint64_t generation) {
        if (source == nullptr || device_ == nullptr ||
            (depth_preview_shader_ == nullptr && ssao_shader_ == nullptr) ||
            generation == 0) {
            return false;
        }
        if (depth_copy_texture_ != nullptr && depth_copy_view_ != nullptr &&
            depth_candidate_generation_ == generation) {
            return true;
        }
        if (depth_failed_generation_ == generation) {
            return false;
        }

        release_depth_capture_resources(false);

        ID3D11Device* source_device = nullptr;
        source->GetDevice(&source_device);
        const bool same_device = source_device == device_;
        safe_release(source_device);
        if (!same_device) {
            log_message(
                "Candidato depth pertence a outro dispositivo; "
                "captura recusada.");
            depth_failed_generation_ = generation;
            return false;
        }

        DXGI_FORMAT resource_format = DXGI_FORMAT_UNKNOWN;
        DXGI_FORMAT view_format = DXGI_FORMAT_UNKNOWN;
        if (source_description.SampleDesc.Count != 1 ||
            source_description.ArraySize != 1 ||
            !depth_copy_formats(
                source_description.Format, &resource_format, &view_format)) {
            log_message(
                "Candidato depth incompativel com copia 0.9.1: "
                "size=%ux%u format=%u samples=%u array=%u.",
                source_description.Width,
                source_description.Height,
                static_cast<unsigned>(source_description.Format),
                source_description.SampleDesc.Count,
                source_description.ArraySize);
            depth_failed_generation_ = generation;
            return false;
        }

        D3D11_TEXTURE2D_DESC copy_description = source_description;
        copy_description.Format = resource_format;
        copy_description.Usage = D3D11_USAGE_DEFAULT;
        copy_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        copy_description.CPUAccessFlags = 0;
        copy_description.MiscFlags = 0;
        HRESULT result = device_->CreateTexture2D(
            &copy_description, nullptr, &depth_copy_texture_);
        if (SUCCEEDED(result) && depth_copy_texture_ != nullptr) {
            D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
            view_description.Format = view_format;
            view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            view_description.Texture2D.MostDetailedMip = 0;
            view_description.Texture2D.MipLevels = 1;
            result = device_->CreateShaderResourceView(
                depth_copy_texture_, &view_description, &depth_copy_view_);
        }
        if (FAILED(result) || depth_copy_texture_ == nullptr ||
            depth_copy_view_ == nullptr) {
            log_message(
                "Falha ao criar copia depth legivel: result=0x%08X "
                "source_format=%u resource_format=%u view_format=%u.",
                static_cast<unsigned>(result),
                static_cast<unsigned>(source_description.Format),
                static_cast<unsigned>(resource_format),
                static_cast<unsigned>(view_format));
            safe_release(depth_copy_view_);
            safe_release(depth_copy_texture_);
            depth_failed_generation_ = generation;
            return false;
        }

        depth_candidate_generation_ = generation;
        depth_failed_generation_ = 0;
        log_message(
            "Recursos de copia depth criados: generation=%llu size=%ux%u "
            "source_format=%u resource_format=%u view_format=%u.",
            static_cast<unsigned long long>(generation),
            source_description.Width,
            source_description.Height,
            static_cast<unsigned>(source_description.Format),
            static_cast<unsigned>(resource_format),
            static_cast<unsigned>(view_format));
        return true;
    }

    bool ensure_temporal_resources(
        const D3D11_TEXTURE2D_DESC& frame_description,
        const D3D11_TEXTURE2D_DESC& depth_description,
        std::uint64_t generation) {
        if (device_ == nullptr || depth_copy_texture_ == nullptr ||
            temporal_shader_ == nullptr || generation == 0) {
            return false;
        }
        if (temporal_history_texture_ != nullptr &&
            temporal_history_view_ != nullptr &&
            temporal_depth_history_texture_ != nullptr &&
            temporal_depth_history_view_ != nullptr &&
            temporal_history_generation_ == generation &&
            temporal_depth_width_ == depth_description.Width &&
            temporal_depth_height_ == depth_description.Height) {
            return true;
        }

        release_temporal_resources();

        D3D11_TEXTURE2D_DESC color_description = frame_description;
        color_description.Usage = D3D11_USAGE_DEFAULT;
        color_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        color_description.CPUAccessFlags = 0;
        color_description.MiscFlags = 0;
        color_description.Format = typeless_format(frame_description.Format);
        HRESULT result = device_->CreateTexture2D(
            &color_description, nullptr, &temporal_history_texture_);
        if (SUCCEEDED(result) && temporal_history_texture_ != nullptr) {
            D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
            view_description.Format = srgb_view_format(frame_description.Format);
            view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            view_description.Texture2D.MostDetailedMip = 0;
            view_description.Texture2D.MipLevels = frame_description.MipLevels;
            result = device_->CreateShaderResourceView(
                temporal_history_texture_,
                &view_description,
                &temporal_history_view_);
        }

        DXGI_FORMAT depth_resource_format = DXGI_FORMAT_UNKNOWN;
        DXGI_FORMAT depth_view_format = DXGI_FORMAT_UNKNOWN;
        if (SUCCEEDED(result) &&
            depth_copy_formats(
                depth_description.Format,
                &depth_resource_format,
                &depth_view_format)) {
            D3D11_TEXTURE2D_DESC history_depth_description = depth_description;
            history_depth_description.Format = depth_resource_format;
            history_depth_description.Usage = D3D11_USAGE_DEFAULT;
            history_depth_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            history_depth_description.CPUAccessFlags = 0;
            history_depth_description.MiscFlags = 0;
            result = device_->CreateTexture2D(
                &history_depth_description,
                nullptr,
                &temporal_depth_history_texture_);
            if (SUCCEEDED(result) &&
                temporal_depth_history_texture_ != nullptr) {
                D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
                view_description.Format = depth_view_format;
                view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                view_description.Texture2D.MostDetailedMip = 0;
                view_description.Texture2D.MipLevels = 1;
                result = device_->CreateShaderResourceView(
                    temporal_depth_history_texture_,
                    &view_description,
                    &temporal_depth_history_view_);
            }
        } else if (SUCCEEDED(result)) {
            result = E_INVALIDARG;
        }

        if (FAILED(result) || temporal_history_texture_ == nullptr ||
            temporal_history_view_ == nullptr ||
            temporal_depth_history_texture_ == nullptr ||
            temporal_depth_history_view_ == nullptr) {
            if (!temporal_resources_failure_logged_) {
                log_message(
                    "Falha ao criar historico temporal 0.10.0: "
                    "result=0x%08X color=%ux%u depth=%ux%u format=%u.",
                    static_cast<unsigned>(result),
                    frame_description.Width,
                    frame_description.Height,
                    depth_description.Width,
                    depth_description.Height,
                    static_cast<unsigned>(depth_description.Format));
                temporal_resources_failure_logged_ = true;
            }
            release_temporal_resources();
            return false;
        }

        temporal_history_generation_ = generation;
        temporal_depth_width_ = depth_description.Width;
        temporal_depth_height_ = depth_description.Height;
        temporal_history_valid_ = false;
        temporal_resources_failure_logged_ = false;
        log_message(
            "Recursos temporais 0.10.0 criados: color=%ux%u depth=%ux%u "
            "generation=%llu.",
            frame_description.Width,
            frame_description.Height,
            depth_description.Width,
            depth_description.Height,
            static_cast<unsigned long long>(generation));
        return true;
    }

    void invalidate_temporal_history(const char* reason) {
        if (!temporal_history_valid_) {
            return;
        }
        temporal_history_valid_ = false;
        log_message(
            "Historico temporal 0.10.0 invalidado: %s.",
            reason != nullptr ? reason : "motivo nao informado");
    }

    void release_temporal_resources() {
        safe_release(temporal_depth_history_view_);
        safe_release(temporal_depth_history_texture_);
        safe_release(temporal_history_view_);
        safe_release(temporal_history_texture_);
        temporal_history_generation_ = 0;
        temporal_depth_width_ = 0;
        temporal_depth_height_ = 0;
        temporal_history_valid_ = false;
        temporal_active_logged_generation_ = 0;
        temporal_wait_logged_ = false;
    }

    void release_depth_capture_resources(bool reset_liveness = true) {
        release_temporal_resources();
        safe_release(depth_copy_view_);
        safe_release(depth_copy_texture_);
        depth_candidate_generation_ = 0;
        depth_failed_generation_ = 0;
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
        release_bloom_resources();

        scene_observer_.release();
        release_scene_textures();
        width_ = 0;
        height_ = 0;
        format_ = DXGI_FORMAT_UNKNOWN;
        scene_needs_srgb_decode_ = false;
        unsupported_logged_ = false;
        processed_logged_ = false;
        temporal_resources_failure_logged_ = false;
        bloom_resources_failure_logged_ = false;
        bloom_active_logged_levels_ = 0;
    }

    void initialize_gpu_timing() {
        release_gpu_timing();
        if (device_ == nullptr || context_ == nullptr) {
            return;
        }

        D3D11_QUERY_DESC description = {};
        for (GpuTimingSlot& slot : gpu_timing_slots_) {
            description.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
            HRESULT result = device_->CreateQuery(&description, &slot.disjoint);
            if (SUCCEEDED(result)) {
                description.Query = D3D11_QUERY_TIMESTAMP;
                result = device_->CreateQuery(&description, &slot.start);
            }
            if (SUCCEEDED(result)) {
                result = device_->CreateQuery(&description, &slot.end);
            }
            if (FAILED(result) || slot.disjoint == nullptr ||
                slot.start == nullptr || slot.end == nullptr) {
                log_message(
                    "Telemetria GPU indisponivel: CreateQuery result=0x%08X.",
                    static_cast<unsigned>(result));
                release_gpu_timing();
                return;
            }
        }

        gpu_timing_available_ = true;
        gpu_report_started_at_ = GetTickCount64();
        log_message(
            "Telemetria GPU inicializada: %u amostras em anel, relatorio=%us.",
            kGpuTimingSlotCount,
            kGpuReportIntervalMilliseconds / 1000);
    }

    void release_gpu_timing() {
        for (GpuTimingSlot& slot : gpu_timing_slots_) {
            safe_release(slot.disjoint);
            safe_release(slot.start);
            safe_release(slot.end);
            slot.pending = false;
        }
        gpu_timing_available_ = false;
        active_gpu_timing_slot_ = -1;
        next_gpu_timing_slot_ = 0;
        gpu_sample_count_ = 0;
        gpu_dropped_sample_count_ = 0;
        gpu_time_sum_ms_ = 0.0;
        gpu_time_min_ms_ = DBL_MAX;
        gpu_time_max_ms_ = 0.0;
        gpu_report_started_at_ = 0;
        gpu_query_error_logged_ = false;
    }

    void poll_gpu_timing() {
        if (!gpu_timing_available_ || context_ == nullptr) {
            return;
        }

        for (GpuTimingSlot& slot : gpu_timing_slots_) {
            if (!slot.pending) {
                continue;
            }

            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint_data = {};
            HRESULT result = context_->GetData(
                slot.disjoint,
                &disjoint_data,
                sizeof(disjoint_data),
                D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (result == S_FALSE) {
                continue;
            }
            if (FAILED(result)) {
                handle_gpu_query_failure(result);
                slot.pending = false;
                continue;
            }

            UINT64 start_timestamp = 0;
            UINT64 end_timestamp = 0;
            const HRESULT start_result = context_->GetData(
                slot.start,
                &start_timestamp,
                sizeof(start_timestamp),
                D3D11_ASYNC_GETDATA_DONOTFLUSH);
            const HRESULT end_result = context_->GetData(
                slot.end,
                &end_timestamp,
                sizeof(end_timestamp),
                D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (start_result == S_FALSE || end_result == S_FALSE) {
                continue;
            }
            if (FAILED(start_result) || FAILED(end_result)) {
                handle_gpu_query_failure(
                    FAILED(start_result) ? start_result : end_result);
                slot.pending = false;
                continue;
            }

            slot.pending = false;
            if (disjoint_data.Disjoint || disjoint_data.Frequency == 0 ||
                end_timestamp < start_timestamp) {
                ++gpu_dropped_sample_count_;
                continue;
            }

            const double milliseconds =
                static_cast<double>(end_timestamp - start_timestamp) * 1000.0 /
                static_cast<double>(disjoint_data.Frequency);
            record_gpu_timing(milliseconds);
        }
    }

    bool begin_gpu_timing() {
        active_gpu_timing_slot_ = -1;
        if (!gpu_timing_available_ || context_ == nullptr) {
            return false;
        }

        for (UINT attempt = 0; attempt < kGpuTimingSlotCount; ++attempt) {
            const UINT index =
                (next_gpu_timing_slot_ + attempt) % kGpuTimingSlotCount;
            GpuTimingSlot& slot = gpu_timing_slots_[index];
            if (slot.pending) {
                continue;
            }
            context_->Begin(slot.disjoint);
            context_->End(slot.start);
            active_gpu_timing_slot_ = static_cast<int>(index);
            next_gpu_timing_slot_ = (index + 1) % kGpuTimingSlotCount;
            return true;
        }

        ++gpu_dropped_sample_count_;
        return false;
    }

    void end_gpu_timing() {
        if (active_gpu_timing_slot_ < 0 || context_ == nullptr) {
            return;
        }
        GpuTimingSlot& slot =
            gpu_timing_slots_[static_cast<UINT>(active_gpu_timing_slot_)];
        context_->End(slot.end);
        context_->End(slot.disjoint);
        slot.pending = true;
        active_gpu_timing_slot_ = -1;
    }

    void handle_gpu_query_failure(HRESULT result) {
        if (!gpu_query_error_logged_) {
            log_message(
                "Falha ao consultar telemetria GPU: 0x%08X; "
                "o passe visual continuara ativo.",
                static_cast<unsigned>(result));
            gpu_query_error_logged_ = true;
        }
        ++gpu_dropped_sample_count_;
    }

    void record_gpu_timing(double milliseconds) {
        if (milliseconds < 0.0 || milliseconds > 1000.0) {
            ++gpu_dropped_sample_count_;
            return;
        }

        ++gpu_sample_count_;
        gpu_time_sum_ms_ += milliseconds;
        if (milliseconds < gpu_time_min_ms_) {
            gpu_time_min_ms_ = milliseconds;
        }
        if (milliseconds > gpu_time_max_ms_) {
            gpu_time_max_ms_ = milliseconds;
        }

        const ULONGLONG now = GetTickCount64();
        if (gpu_report_started_at_ == 0) {
            gpu_report_started_at_ = now;
        }
        if (now - gpu_report_started_at_ <
                kGpuReportIntervalMilliseconds ||
            gpu_sample_count_ == 0) {
            return;
        }

        log_message(
            "Custo GPU do passe: media=%.3f ms minimo=%.3f ms pico=%.3f ms "
            "amostras=%llu descartadas=%llu.",
            gpu_time_sum_ms_ / static_cast<double>(gpu_sample_count_),
            gpu_time_min_ms_,
            gpu_time_max_ms_,
            static_cast<unsigned long long>(gpu_sample_count_),
            static_cast<unsigned long long>(gpu_dropped_sample_count_));

        gpu_sample_count_ = 0;
        gpu_dropped_sample_count_ = 0;
        gpu_time_sum_ms_ = 0.0;
        gpu_time_min_ms_ = DBL_MAX;
        gpu_time_max_ms_ = 0.0;
        gpu_report_started_at_ = now;
    }

    void reset_device() {
        shutdown_steam_screenshots();
        release_frame_resources();
        release_gpu_timing();
        safe_release(vertex_shader_);
        safe_release(pixel_shader_);
        safe_release(depth_preview_shader_);
        safe_release(ssao_shader_);
        safe_release(temporal_shader_);
        safe_release(bloom_bright_shader_);
        safe_release(bloom_downsample_shader_);
        safe_release(bloom_upsample_shader_);
        safe_release(constant_buffer_);
        safe_release(depth_constant_buffer_);
        safe_release(ssao_constant_buffer_);
        safe_release(temporal_constant_buffer_);
        safe_release(bloom_constant_buffer_);
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
    static constexpr UINT kBloomMaxLevels = 6;
    ID3D11Texture2D* bloom_textures_[kBloomMaxLevels] = {};
    ID3D11ShaderResourceView* bloom_views_[kBloomMaxLevels] = {};
    ID3D11RenderTargetView* bloom_targets_[kBloomMaxLevels] = {};
    UINT bloom_widths_[kBloomMaxLevels] = {};
    UINT bloom_heights_[kBloomMaxLevels] = {};
    UINT bloom_level_count_ = 0;
    ID3D11Texture2D* spatial_texture_ = nullptr;
    ID3D11ShaderResourceView* spatial_view_ = nullptr;
    ID3D11RenderTargetView* spatial_target_ = nullptr;
    ID3D11Texture2D* depth_copy_texture_ = nullptr;
    ID3D11ShaderResourceView* depth_copy_view_ = nullptr;

    ID3D11Texture2D* temporal_history_texture_ = nullptr;
    ID3D11ShaderResourceView* temporal_history_view_ = nullptr;
    ID3D11Texture2D* temporal_depth_history_texture_ = nullptr;
    ID3D11ShaderResourceView* temporal_depth_history_view_ = nullptr;
    ID3D11VertexShader* vertex_shader_ = nullptr;
    ID3D11PixelShader* pixel_shader_ = nullptr;
    ID3D11PixelShader* depth_preview_shader_ = nullptr;
    ID3D11PixelShader* ssao_shader_ = nullptr;
    ID3D11PixelShader* temporal_shader_ = nullptr;
    ID3D11PixelShader* bloom_bright_shader_ = nullptr;
    ID3D11PixelShader* bloom_downsample_shader_ = nullptr;
    ID3D11PixelShader* bloom_upsample_shader_ = nullptr;
    ID3D11Buffer* constant_buffer_ = nullptr;
    ID3D11Buffer* depth_constant_buffer_ = nullptr;
    ID3D11Buffer* ssao_constant_buffer_ = nullptr;
    ID3D11Buffer* temporal_constant_buffer_ = nullptr;
    ID3D11Buffer* bloom_constant_buffer_ = nullptr;
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
    bool temporal_resources_failure_logged_ = false;
    bool bloom_resources_failure_logged_ = false;
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
    std::uint64_t depth_candidate_generation_ = 0;
    std::uint64_t depth_failed_generation_ = 0;
    std::uint64_t ssao_active_logged_generation_ = 0;
    std::uint64_t temporal_history_generation_ = 0;
    std::uint64_t temporal_active_logged_generation_ = 0;
    UINT bloom_active_logged_levels_ = 0;
    UINT temporal_depth_width_ = 0;
    UINT temporal_depth_height_ = 0;
    bool temporal_history_valid_ = false;
    std::uint64_t depth_liveness_generation_ = 0;
    std::uint64_t depth_last_binding_serial_ = 0;
    UINT depth_stale_frame_count_ = 0;
    bool depth_stale_logged_ = false;
    static constexpr UINT kDepthActivityGraceFrames = 2;
    static constexpr UINT kDepthStaleFrameThreshold = 30;
    static constexpr UINT kGpuTimingSlotCount = 8;
    static constexpr ULONGLONG kGpuReportIntervalMilliseconds = 10000;
    GpuTimingSlot gpu_timing_slots_[kGpuTimingSlotCount] = {};
    bool gpu_timing_available_ = false;
    bool gpu_query_error_logged_ = false;
    int active_gpu_timing_slot_ = -1;
    UINT next_gpu_timing_slot_ = 0;
    std::uint64_t gpu_sample_count_ = 0;
    std::uint64_t gpu_dropped_sample_count_ = 0;
    double gpu_time_sum_ms_ = 0.0;
    double gpu_time_min_ms_ = DBL_MAX;
    double gpu_time_max_ms_ = 0.0;
    ULONGLONG gpu_report_started_at_ = 0;
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
