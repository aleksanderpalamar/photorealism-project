#include "postprocess.hpp"

#include "config.hpp"
#include "resource_observer.hpp"
#include "runtime.hpp"
#include "scene_observer.hpp"
#include "steam_screenshots.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <windows.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
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
    // 0.17.1: piso do preto por canal. A linha de 16 bytes cabe float3 + um
    // float, entao highlight_rolloff fica aqui e tint desce para a linha do
    // bloom -- o buffer continua com 96 bytes e a ordem espelha o cbuffer.
    float black_lift[3];
    float highlight_rolloff;
    float tint;
    float bloom_enabled;
    float bloom_intensity;
    float bloom_padding;
};

static_assert(sizeof(ShaderConstants) == 96, "constant buffer must be aligned");

// Os passes do bloom. SourceTexelSize e o texel da textura LIDA, e muda a cada
// passo da piramide -- por isso o buffer e reescrito entre draws em vez de uma
// vez por frame como os outros.
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

// A ordem importa: o indice e usado direto em bloom_bright_shader_,
// bloom_downsample_shader_ e bloom_upsample_shader_, nesta sequencia.
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

    void render(IDXGISwapChain* swap_chain) {
        if (swap_chain == nullptr) {
            return;
        }
        if (resize_in_progress_) {
            return;
        }

        if (active_swap_chain_ != swap_chain) {
            release_frame_resources();
            active_swap_chain_ = swap_chain;
            log_message(
                "Swap chain ativo detectado: %p; recursos serao vinculados "
                "no proximo frame.",
                static_cast<void*>(swap_chain));
        }

        if (key_pressed_once(VK_HOME, &home_key_down_)) {
            settings_.enabled = !settings_.enabled;
            invalidate_temporal_history("alternancia Home");
            log_message(
                "Efeito %s pelo atalho Home.",
                settings_.enabled ? "ativado" : "desativado");
        }
        if (key_pressed_once(VK_END, &end_key_down_)) {
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
        if (key_pressed_once(VK_INSERT, &insert_key_down_)) {
            depth_preview_mode_ = (depth_preview_mode_ + 1) % 7;
            invalidate_temporal_history("mudanca de preview Insert");
            depth_preview_wait_logged_ = false;
            depth_preview_logged_mode_ = 0;
            const char* mode_name = "normal";
            if (depth_preview_mode_ == 1) {
                mode_name = "raw";
            } else if (depth_preview_mode_ == 2) {
                mode_name = "reversed-z-enhanced";
            } else if (depth_preview_mode_ == 3) {
                mode_name = "linear-distance";
            } else if (depth_preview_mode_ == 4) {
                mode_name = "reconstructed-normals";
            } else if (depth_preview_mode_ == 5) {
                mode_name = "ssao-visibility";
            } else if (depth_preview_mode_ == 6) {
                mode_name = "bloom";
            }
            log_message(
                "Preview depth solicitado pelo Insert: mode=%u(%s).",
                depth_preview_mode_,
                mode_name);
        }
        if (!settings_.enabled) {
            return;
        }

        ID3D11Device* frame_device = nullptr;
        HRESULT result = swap_chain->GetDevice(
            IID_ID3D11Device, reinterpret_cast<void**>(&frame_device));
        if (FAILED(result) || frame_device == nullptr) {
            return;
        }

        if (device_ != frame_device) {
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
                safe_release(frame_device);
                return;
            }
            log_message("Pipeline D3D11 inicializado.");
        }
        safe_release(frame_device);

        poll_gpu_timing();

        ID3D11Texture2D* back_buffer = nullptr;
        result = swap_chain->GetBuffer(
            0, IID_ID3D11Texture2D, reinterpret_cast<void**>(&back_buffer));
        if (FAILED(result) || back_buffer == nullptr) {
            return;
        }

        D3D11_TEXTURE2D_DESC description = {};
        back_buffer->GetDesc(&description);
        if (description.Width < 640 || description.Height < 480 ||
            description.SampleDesc.Count != 1 ||
            !is_supported_format(description.Format)) {
            if (!unsupported_logged_) {
                log_message(
                    "Backbuffer ignorado: %ux%u format=%u samples=%u.",
                    description.Width,
                    description.Height,
                    static_cast<unsigned>(description.Format),
                    description.SampleDesc.Count);
                unsupported_logged_ = true;
            }
            safe_release(back_buffer);
            return;
        }

        update_backbuffer_signature(
            description.Width, description.Height, description.Format);

        if (!ensure_frame_resources(description)) {
            safe_release(back_buffer);
            return;
        }

        ID3D11RenderTargetView* output = nullptr;
        D3D11_RENDER_TARGET_VIEW_DESC output_description = {};
        output_description.Format = srgb_view_format(description.Format);
        output_description.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        output_description.Texture2D.MipSlice = 0;
        result = device_->CreateRenderTargetView(
            back_buffer, &output_description, &output);
        bool output_needs_srgb_encode = false;
        if (FAILED(result) || output == nullptr) {
            result = device_->CreateRenderTargetView(
                back_buffer, nullptr, &output);
            output_needs_srgb_encode = is_unorm_format(description.Format);
            if (!output_fallback_logged_) {
                log_message(
                    "RTV sRGB indisponivel; usando codificacao manual na saida.");
                output_fallback_logged_ = true;
            }
        }
        if (FAILED(result) || output == nullptr) {
            log_message("Falha ao criar RTV do backbuffer: 0x%08X.", static_cast<unsigned>(result));
            safe_release(back_buffer);
            return;
        }

        SavedState state = {};
        capture_state(context_, &state);

        const bool gpu_timing_active = begin_gpu_timing();

        context_->OMSetRenderTargets(0, nullptr, nullptr);
        bool depth_available = false;
        bool depth_preview_active = false;
        bool ssao_preview_active = false;
        bool ssao_active = false;
        bool temporal_active = false;
        bool bloom_active = false;
        bool bloom_preview_active = false;
        ID3D11Texture2D* depth_candidate = nullptr;
        D3D11_TEXTURE2D_DESC depth_description = {};
        std::uint64_t depth_generation = 0;
        std::uint64_t depth_binding_serial = 0;
        bool depth_candidate_invalidated = false;
        // O modo 6 e o preview do bloom, que sai da cena e nao do depth. Ele
        // fica de fora daqui para nao disparar a copia do depth a toa.
        const bool depth_requested =
            settings_.ssao_enabled || settings_.temporal_enabled ||
            (depth_preview_mode_ != 0 && depth_preview_mode_ != 6);
        if (depth_requested &&
            acquire_depth_candidate(
                &depth_candidate,
                &depth_description,
                &depth_generation,
                &depth_binding_serial)) {
            if (depth_liveness_generation_ != depth_generation) {
                depth_liveness_generation_ = depth_generation;
                depth_last_binding_serial_ = 0;
                depth_stale_frame_count_ = 0;
                depth_stale_logged_ = false;
            }

            const bool depth_used_by_current_scene =
                depth_binding_serial != 0 &&
                depth_binding_serial != depth_last_binding_serial_;
            if (depth_used_by_current_scene) {
                if (depth_stale_logged_) {
                    log_message(
                        "Depth voltou a ser usado pela cena: generation=%llu; "
                        "SSAO photorealista retomado.",
                        static_cast<unsigned long long>(depth_generation));
                }
                depth_last_binding_serial_ = depth_binding_serial;
                depth_stale_frame_count_ = 0;
                depth_stale_logged_ = false;
            } else {
                if (depth_stale_frame_count_ < kDepthStaleFrameThreshold) {
                    ++depth_stale_frame_count_;
                }
                if (depth_stale_frame_count_ ==
                        kDepthActivityGraceFrames + 1 &&
                    !depth_stale_logged_) {
                    log_message(
                        "Depth sem atividade confirmada por %u frames: "
                        "generation=%llu; SSAO suspenso e visual "
                        "photorealista preservado.",
                        depth_stale_frame_count_,
                        static_cast<unsigned long long>(depth_generation));
                    depth_stale_logged_ = true;
                }
                if (depth_stale_frame_count_ >= kDepthStaleFrameThreshold &&
                    invalidate_stale_depth_candidate(
                        depth_generation, depth_binding_serial)) {
                    log_message(
                        "Depth obsoleto invalidado apos %u frames; "
                        "redescoberta automatica iniciada sem acao do usuario.",
                        kDepthStaleFrameThreshold);
                    release_depth_capture_resources();
                    depth_candidate_invalidated = true;
                }
            }

            const bool depth_safe_for_scene =
                !depth_candidate_invalidated &&
                (depth_used_by_current_scene ||
                 depth_stale_frame_count_ <= kDepthActivityGraceFrames);
            if (depth_safe_for_scene &&
                ensure_depth_capture_resources(
                    depth_candidate,
                    depth_description,
                    depth_generation)) {
                context_->CopyResource(depth_copy_texture_, depth_candidate);
                depth_available = true;
            }
        }
        safe_release(depth_candidate);

        depth_preview_active =
            depth_available && depth_preview_mode_ >= 1 &&
            depth_preview_mode_ <= 4 && depth_preview_shader_ != nullptr;
        ssao_preview_active =
            depth_available && depth_preview_mode_ == 5 &&
            ssao_shader_ != nullptr;
        ssao_active =
            depth_available && depth_preview_mode_ == 0 &&
            settings_.ssao_enabled && ssao_shader_ != nullptr &&
            visual_target_ != nullptr && visual_view_ != nullptr;
        if (depth_available && depth_preview_mode_ == 0 &&
            settings_.temporal_enabled && temporal_shader_ != nullptr &&
            visual_target_ != nullptr && visual_view_ != nullptr) {
            temporal_active = ensure_temporal_resources(
                description, depth_description, depth_generation);
            if (ssao_active &&
                (spatial_target_ == nullptr || spatial_view_ == nullptr)) {
                temporal_active = false;
            }
        }

        if (!temporal_active && temporal_history_valid_) {
            invalidate_temporal_history("depth ou passe temporal indisponivel");
        }

        // O bloom nao depende de depth -- ele sai da propria cena. E a unica
        // coisa da pilha que continua funcionando quando a descoberta de depth
        // falha, e por isso a condicao aqui nao menciona depth_available.
        bloom_preview_active =
            depth_preview_mode_ == 6 && bloom_bright_shader_ != nullptr &&
            additive_blend_state_ != nullptr;
        if ((depth_preview_mode_ == 0 || bloom_preview_active) &&
            settings_.bloom_enabled && bloom_bright_shader_ != nullptr &&
            additive_blend_state_ != nullptr) {
            bloom_active = ensure_bloom_resources(
                description,
                bloom_levels_for_radius(
                    settings_.bloom_radius, description.Height));
        }
        if (!bloom_active) {
            bloom_preview_active = false;
            if (settings_.bloom_enabled && depth_preview_mode_ == 0 &&
                !bloom_wait_logged_) {
                log_message(
                    "Bloom 0.17.0 aguardando shaders e recursos validos; "
                    "a pilha visual aprovada permanece ativa.");
                bloom_wait_logged_ = true;
            }
        } else {
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
        }

        if (depth_preview_active) {
            depth_preview_wait_logged_ = false;
            if (depth_preview_logged_mode_ != depth_preview_mode_) {
                log_message(
                    "Preview depth ativo: mode=%u source=%ux%u "
                    "format=%u generation=%llu near=%.4f range=%.1f "
                    "vertical_fov=%.1f.",
                    depth_preview_mode_,
                    depth_description.Width,
                    depth_description.Height,
                    static_cast<unsigned>(depth_description.Format),
                    static_cast<unsigned long long>(depth_generation),
                    settings_.depth_near_plane,
                    settings_.depth_preview_distance,
                    settings_.depth_vertical_fov);
                depth_preview_logged_mode_ = depth_preview_mode_;
            }
        } else if (ssao_preview_active) {
            depth_preview_wait_logged_ = false;
            if (depth_preview_logged_mode_ != depth_preview_mode_) {
                log_message(
                    "Preview SSAO ativo: mode=5 source=%ux%u generation=%llu "
                    "radius=%.3f intensity=%.3f.",
                    depth_description.Width,
                    depth_description.Height,
                    static_cast<unsigned long long>(depth_generation),
                    settings_.ssao_radius,
                    settings_.ssao_intensity);
                depth_preview_logged_mode_ = depth_preview_mode_;
            }
        }

        if (ssao_active) {
            ssao_wait_logged_ = false;
            if (ssao_active_logged_generation_ != depth_generation) {
                log_message(
                    "SSAO 0.9.1 ativo: source=%ux%u format=%u "
                    "generation=%llu samples=%u radius=%.3f intensity=%.3f "
                    "fade=%.1f-%.1f interior=%s.",
                    depth_description.Width,
                    depth_description.Height,
                    static_cast<unsigned>(depth_description.Format),
                    static_cast<unsigned long long>(depth_generation),
                    settings_.ssao_refinement_enabled ? 16u : 8u,
                    settings_.ssao_radius,
                    settings_.ssao_intensity,
                    settings_.ssao_fade_start,
                    settings_.ssao_fade_end,
                    settings_.ssao_interior_enabled ? "ativo" : "inativo");
                ssao_active_logged_generation_ = depth_generation;
            }
        } else if (depth_preview_mode_ == 0 && settings_.ssao_enabled &&
                   !ssao_wait_logged_) {
            log_message(
                "SSAO 0.9.1 aguardando depth e recursos validos; "
                "o passe visual aprovado permanece ativo.");
            ssao_wait_logged_ = true;
        }

        if (temporal_active) {
            temporal_wait_logged_ = false;
            if (temporal_active_logged_generation_ != depth_generation) {
                log_message(
                    "Resolve temporal 0.10.0 ativo: source=%ux%u "
                    "depth=%ux%u generation=%llu history_weight=%.2f "
                    "depth_rejection=%.3f color_rejection=%.3f.",
                    description.Width,
                    description.Height,
                    depth_description.Width,
                    depth_description.Height,
                    static_cast<unsigned long long>(depth_generation),
                    settings_.temporal_history_weight,
                    settings_.temporal_depth_rejection,
                    settings_.temporal_color_rejection);
                temporal_active_logged_generation_ = depth_generation;
            }
        } else if (depth_preview_mode_ == 0 && settings_.temporal_enabled &&
                   !temporal_wait_logged_) {
            log_message(
                "Resolve temporal 0.10.0 aguardando depth e recursos validos; "
                "pilha visual/SSAO permanece ativa.");
            temporal_wait_logged_ = true;
        }


        if (!depth_preview_active) {
            context_->CopyResource(scene_texture_, back_buffer);
            // 0.18.0. Aqui, e so aqui: scene_texture_ acabou de receber o
            // frame do jogo e nenhum passe nosso escreveu nele ainda. Medir
            // depois do grade fecharia uma realimentacao -- a cor seria funcao
            // das features e as features funcao da cor -- e a imagem
            // caminharia sozinha sem que nada no cfg tivesse mudado.
            scene_observer_.observe(device_, context_, scene_texture_);
            // errada de que o depth faltava no modo 6.
            if (depth_preview_mode_ != 0 && !ssao_preview_active &&
                !bloom_preview_active && !depth_preview_wait_logged_) {
                log_message(
                    "Diagnostico depth/SSAO aguardando candidato valido; "
                    "o passe visual normal permanece ativo.");
                depth_preview_wait_logged_ = true;
            }
        }

        ShaderConstants constants = {};
        constants.texel_size[0] = 1.0f / static_cast<float>(description.Width);
        constants.texel_size[1] = 1.0f / static_cast<float>(description.Height);
        constants.exposure = settings_.exposure;
        constants.temperature = settings_.temperature;
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
        constants.tint = settings_.tint;
        constants.bloom_enabled = bloom_active ? 1.0f : 0.0f;
        constants.bloom_intensity = settings_.bloom_intensity;
        constants.input_needs_srgb_decode =
            scene_needs_srgb_decode_ ? 1.0f : 0.0f;
        constants.output_needs_srgb_encode =
            (ssao_active || temporal_active) ? 0.0f :
            (output_needs_srgb_encode ? 1.0f : 0.0f);
        context_->UpdateSubresource(constant_buffer_, 0, nullptr, &constants, 0, 0);

        const UINT depth_width =
            depth_available ? depth_description.Width : description.Width;
        const UINT depth_height =
            depth_available ? depth_description.Height : description.Height;
        const float depth_texel_x = 1.0f / static_cast<float>(depth_width);
        const float depth_texel_y = 1.0f / static_cast<float>(depth_height);
        constexpr float pi = 3.14159265358979323846f;
        const float vertical_fov_radians =
            settings_.depth_vertical_fov * pi / 180.0f;
        const float projection_y =
            1.0f / std::tan(vertical_fov_radians * 0.5f);
        const float output_aspect =
            static_cast<float>(description.Width) /
            static_cast<float>(description.Height);
        const float projection_x = projection_y / output_aspect;

        DepthPreviewConstants depth_constants = {};
        depth_constants.preview_mode =
            static_cast<float>(depth_preview_mode_);
        depth_constants.output_needs_srgb_encode =
            output_needs_srgb_encode ? 1.0f : 0.0f;
        depth_constants.near_plane = settings_.depth_near_plane;
        depth_constants.preview_distance = settings_.depth_preview_distance;
        depth_constants.texel_size[0] = depth_texel_x;
        depth_constants.texel_size[1] = depth_texel_y;
        depth_constants.projection_scale[0] = projection_x;
        depth_constants.projection_scale[1] = projection_y;
        context_->UpdateSubresource(
            depth_constant_buffer_, 0, nullptr, &depth_constants, 0, 0);

        SsaoConstants ssao_constants = {};
        ssao_constants.input_needs_srgb_decode =
            ssao_preview_active && scene_needs_srgb_decode_ ? 1.0f : 0.0f;
        ssao_constants.output_needs_srgb_encode =
            temporal_active ? 0.0f :
            (output_needs_srgb_encode ? 1.0f : 0.0f);
        ssao_constants.near_plane = settings_.depth_near_plane;
        ssao_constants.radius = settings_.ssao_radius;
        ssao_constants.intensity = settings_.ssao_intensity;
        ssao_constants.bias = settings_.ssao_bias;
        ssao_constants.fade_start = settings_.ssao_fade_start;
        ssao_constants.fade_end = settings_.ssao_fade_end;
        ssao_constants.edge_rejection = settings_.ssao_edge_rejection;
        ssao_constants.debug_mode = ssao_preview_active ? 1.0f : 0.0f;
        ssao_constants.depth_texel_size[0] = depth_texel_x;
        ssao_constants.depth_texel_size[1] = depth_texel_y;
        ssao_constants.projection_scale[0] = projection_x;
        ssao_constants.projection_scale[1] = projection_y;
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

        TemporalConstants temporal_constants = {};
        temporal_constants.texel_size[0] =
            1.0f / static_cast<float>(description.Width);
        temporal_constants.texel_size[1] =
            1.0f / static_cast<float>(description.Height);
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
            output_needs_srgb_encode ? 1.0f : 0.0f;
        context_->UpdateSubresource(
            temporal_constant_buffer_,
            0,
            nullptr,
            &temporal_constants,
            0,
            0);


        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(description.Width);
        viewport.Height = static_cast<float>(description.Height);
        viewport.MaxDepth = 1.0f;

        context_->OMSetBlendState(blend_state_, nullptr, 0xFFFFFFFFu);
        context_->OMSetDepthStencilState(depth_state_, 0);
        context_->RSSetState(rasterizer_state_);
        context_->RSSetViewports(1, &viewport);
        context_->IASetInputLayout(nullptr);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(vertex_shader_, nullptr, 0);
        context_->GSSetShader(nullptr, nullptr, 0);
        context_->HSSetShader(nullptr, nullptr, 0);
        context_->DSSetShader(nullptr, nullptr, 0);

        // A piramide roda ANTES do if/else de cinco ramos, e nao dentro dele.
        // Os cinco ramos comecam todos pelo mesmo passe visual, entao gerar o
        // bloom aqui e le-lo em t1 no PSMain faz o modulo valer para todos de
        // uma vez -- sem multiplicar ramo nenhum. Foi por multiplicar ramos
        // que o modulo de tracado de raios removido na 0.16.0 chegou a 377
        // referencias neste arquivo, e por isso levou uma versao inteira para
        // sair.
        if (bloom_active) {
            render_bloom_pyramid(description);
        }

        if (bloom_preview_active) {
            // Preview do Insert: o buffer do bloom sozinho, sem cena por
            // baixo. E o que permite julgar limiar e raio isolados, do mesmo
            // jeito que a mascara do SSAO na posicao 5.
            context_->OMSetRenderTargets(1, &output, nullptr);
            context_->PSSetShader(bloom_upsample_shader_, nullptr, 0);
            BloomConstants preview_constants = {};
            preview_constants.source_texel_size[0] =
                1.0f / static_cast<float>(bloom_widths_[0]);
            preview_constants.source_texel_size[1] =
                1.0f / static_cast<float>(bloom_heights_[0]);
            preview_constants.threshold = settings_.bloom_threshold;
            preview_constants.knee = settings_.bloom_knee;
            preview_constants.output_needs_srgb_encode =
                output_needs_srgb_encode ? 1.0f : 0.0f;
            context_->UpdateSubresource(
                bloom_constant_buffer_, 0, nullptr, &preview_constants, 0, 0);
            context_->PSSetShaderResources(0, 1, &bloom_views_[0]);
            context_->PSSetSamplers(0, 1, &sampler_state_);
            context_->PSSetConstantBuffers(0, 1, &bloom_constant_buffer_);
            context_->Draw(3, 0);
        } else if (depth_preview_active) {
            context_->OMSetRenderTargets(1, &output, nullptr);
            context_->PSSetShader(depth_preview_shader_, nullptr, 0);
            context_->PSSetShaderResources(0, 1, &depth_copy_view_);
            context_->PSSetSamplers(0, 1, &depth_sampler_state_);
            context_->PSSetConstantBuffers(0, 1, &depth_constant_buffer_);
            context_->Draw(3, 0);
        } else if (ssao_preview_active) {
            context_->OMSetRenderTargets(1, &output, nullptr);
            context_->PSSetShader(ssao_shader_, nullptr, 0);
            ID3D11ShaderResourceView* ssao_resources[2] = {
                scene_view_, depth_copy_view_};
            ID3D11SamplerState* ssao_samplers[2] = {
                sampler_state_, depth_sampler_state_};
            context_->PSSetShaderResources(0, 2, ssao_resources);
            context_->PSSetSamplers(0, 2, ssao_samplers);
            context_->PSSetConstantBuffers(0, 1, &ssao_constant_buffer_);
            context_->Draw(3, 0);
        } else if (temporal_active) {
            context_->OMSetRenderTargets(1, &visual_target_, nullptr);
            context_->PSSetShader(pixel_shader_, nullptr, 0);
            ID3D11ShaderResourceView* visual_resources[2] = {
                scene_view_, bloom_active ? bloom_views_[0] : nullptr};
            context_->PSSetShaderResources(0, 2, visual_resources);
            context_->PSSetSamplers(0, 1, &sampler_state_);
            context_->PSSetConstantBuffers(0, 1, &constant_buffer_);
            context_->Draw(3, 0);

            context_->OMSetRenderTargets(0, nullptr, nullptr);
            ID3D11ShaderResourceView* temporal_current_view = visual_view_;
            if (ssao_active) {
                context_->OMSetRenderTargets(1, &spatial_target_, nullptr);
                context_->PSSetShader(ssao_shader_, nullptr, 0);
                ID3D11ShaderResourceView* ssao_resources[2] = {
                    visual_view_, depth_copy_view_};
                ID3D11SamplerState* ssao_samplers[2] = {
                    sampler_state_, depth_sampler_state_};
                context_->PSSetShaderResources(0, 2, ssao_resources);
                context_->PSSetSamplers(0, 2, ssao_samplers);
                context_->PSSetConstantBuffers(0, 1, &ssao_constant_buffer_);
                context_->Draw(3, 0);
                context_->OMSetRenderTargets(0, nullptr, nullptr);
                ID3D11ShaderResourceView* null_ssao_resources[2] = {};
                context_->PSSetShaderResources(0, 2, null_ssao_resources);
                temporal_current_view = spatial_view_;
            }

            context_->OMSetRenderTargets(1, &output, nullptr);
            context_->PSSetShader(temporal_shader_, nullptr, 0);
            ID3D11ShaderResourceView* temporal_resources[4] = {
                temporal_current_view,
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
            context_->CopyResource(temporal_history_texture_, back_buffer);
            context_->CopyResource(
                temporal_depth_history_texture_, depth_copy_texture_);
            if (!temporal_history_valid_) {
                temporal_history_valid_ = true;
                log_message(
                    "Historico temporal 0.10.0 inicializado: color=%ux%u "
                    "depth=%ux%u generation=%llu.",
                    description.Width,
                    description.Height,
                    depth_description.Width,
                    depth_description.Height,
                    static_cast<unsigned long long>(depth_generation));
            }
        } else if (ssao_active) {
            context_->OMSetRenderTargets(1, &visual_target_, nullptr);
            context_->PSSetShader(pixel_shader_, nullptr, 0);
            ID3D11ShaderResourceView* visual_resources[2] = {
                scene_view_, bloom_active ? bloom_views_[0] : nullptr};
            context_->PSSetShaderResources(0, 2, visual_resources);
            context_->PSSetSamplers(0, 1, &sampler_state_);
            context_->PSSetConstantBuffers(0, 1, &constant_buffer_);
            context_->Draw(3, 0);

            context_->OMSetRenderTargets(0, nullptr, nullptr);
            context_->OMSetRenderTargets(1, &output, nullptr);
            context_->PSSetShader(ssao_shader_, nullptr, 0);
            ID3D11ShaderResourceView* ssao_resources[2] = {
                visual_view_, depth_copy_view_};
            ID3D11SamplerState* ssao_samplers[2] = {
                sampler_state_, depth_sampler_state_};
            context_->PSSetShaderResources(0, 2, ssao_resources);
            context_->PSSetSamplers(0, 2, ssao_samplers);
            context_->PSSetConstantBuffers(0, 1, &ssao_constant_buffer_);
            context_->Draw(3, 0);
        } else {
            context_->OMSetRenderTargets(1, &output, nullptr);
            context_->PSSetShader(pixel_shader_, nullptr, 0);
            ID3D11ShaderResourceView* visual_resources[2] = {
                scene_view_, bloom_active ? bloom_views_[0] : nullptr};
            context_->PSSetShaderResources(0, 2, visual_resources);
            context_->PSSetSamplers(0, 1, &sampler_state_);
            context_->PSSetConstantBuffers(0, 1, &constant_buffer_);
            context_->Draw(3, 0);
        }

        if (gpu_timing_active) {
            end_gpu_timing();
        }

        if (!processed_logged_) {
            log_message(
                "Primeiro frame processado: %ux%u format=%u "
                "srgb_manual_entrada=%s srgb_manual_saida=%s "
                "ssao_solicitado=%s temporal_solicitado=%s.",
                description.Width,
                description.Height,
                static_cast<unsigned>(description.Format),
                scene_needs_srgb_decode_ ? "sim" : "nao",
                output_needs_srgb_encode ? "sim" : "nao",
                settings_.ssao_enabled ? "sim" : "nao",
                settings_.temporal_enabled ? "sim" : "nao");
            processed_logged_ = true;
        }

        ID3D11ShaderResourceView* null_resources[4] = {};
        context_->PSSetShaderResources(0, 4, null_resources);
        restore_state(context_, &state);

        safe_release(output);
        safe_release(back_buffer);
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
        // Cada familia entra pelos DOIS nomes: o tipado (D*) e o typeless
        // pai. A tabela so tinha os tipados ate a 0.18.1, e o ETS2 declara o
        // depth ora como 20 (D32_FLOAT_S8X24_UINT), ora como 19
        // (R32G8X24_TYPELESS) -- o log tem as duas coisas no mesmo dia. No
        // segundo caso o SSAO e o resolve temporal ficavam desligados a
        // sessao inteira, e o candidato recusado era justo o MELHOR dos dois:
        // 19 vem com bind_flags=0x48, ou seja ja e legivel por shader, contra
        // 0x40 do 20. O destino da copia e o mesmo nos dois casos.
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

    bool initialize_pipeline() {
        D3D11_BUFFER_DESC buffer_description = {};
        buffer_description.ByteWidth = sizeof(ShaderConstants);
        buffer_description.Usage = D3D11_USAGE_DEFAULT;
        buffer_description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        HRESULT result = device_->CreateBuffer(
            &buffer_description, nullptr, &constant_buffer_);
        if (FAILED(result)) {
            log_message("Falha ao criar constant buffer: 0x%08X.", static_cast<unsigned>(result));
            return false;
        }

        buffer_description.ByteWidth = sizeof(DepthPreviewConstants);
        result = device_->CreateBuffer(
            &buffer_description, nullptr, &depth_constant_buffer_);
        if (FAILED(result)) {
            log_message(
                "Falha ao criar constant buffer depth: 0x%08X.",
                static_cast<unsigned>(result));
            return false;
        }

        buffer_description.ByteWidth = sizeof(SsaoConstants);
        result = device_->CreateBuffer(
            &buffer_description, nullptr, &ssao_constant_buffer_);
        if (FAILED(result)) {
            log_message(
                "Falha ao criar constant buffer SSAO: 0x%08X.",
                static_cast<unsigned>(result));
            return false;
        }

        buffer_description.ByteWidth = sizeof(TemporalConstants);
        result = device_->CreateBuffer(
            &buffer_description, nullptr, &temporal_constant_buffer_);
        if (FAILED(result)) {
            log_message(
                "Falha ao criar constant buffer temporal: 0x%08X.",
                static_cast<unsigned>(result));
            return false;
        }

        buffer_description.ByteWidth = sizeof(BloomConstants);
        result = device_->CreateBuffer(
            &buffer_description, nullptr, &bloom_constant_buffer_);
        if (FAILED(result)) {
            log_message(
                "Falha ao criar constant buffer bloom: 0x%08X.",
                static_cast<unsigned>(result));
            return false;
        }

        D3D11_SAMPLER_DESC sampler_description = {};
        sampler_description.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler_description.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_description.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_description.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler_description.MaxLOD = FLT_MAX;
        result = device_->CreateSamplerState(
            &sampler_description, &sampler_state_);
        if (FAILED(result)) {
            log_message("Falha ao criar sampler: 0x%08X.", static_cast<unsigned>(result));
            return false;
        }

        sampler_description.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        result = device_->CreateSamplerState(
            &sampler_description, &depth_sampler_state_);
        if (FAILED(result)) {
            log_message(
                "Falha ao criar sampler depth: 0x%08X.",
                static_cast<unsigned>(result));
            return false;
        }

        D3D11_RASTERIZER_DESC rasterizer_description = {};
        rasterizer_description.FillMode = D3D11_FILL_SOLID;
        rasterizer_description.CullMode = D3D11_CULL_NONE;
        rasterizer_description.DepthClipEnable = TRUE;
        result = device_->CreateRasterizerState(
            &rasterizer_description, &rasterizer_state_);
        if (FAILED(result)) {
            log_message("Falha ao criar rasterizer state: 0x%08X.", static_cast<unsigned>(result));
            return false;
        }

        D3D11_DEPTH_STENCIL_DESC depth_description = {};
        depth_description.DepthEnable = FALSE;
        depth_description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depth_description.DepthFunc = D3D11_COMPARISON_ALWAYS;
        result = device_->CreateDepthStencilState(
            &depth_description, &depth_state_);
        if (FAILED(result)) {
            log_message("Falha ao criar depth state: 0x%08X.", static_cast<unsigned>(result));
            return false;
        }

        D3D11_BLEND_DESC blend_description = {};
        blend_description.RenderTarget[0].RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_ALL;
        result = device_->CreateBlendState(&blend_description, &blend_state_);
        if (FAILED(result)) {
            log_message("Falha ao criar blend state: 0x%08X.", static_cast<unsigned>(result));
            return false;
        }

        // A subida da piramide SOMA no nivel de baixo em vez de substituir; e
        // isso que empilha as escalas num unico glow com queda suave. Sem o
        // blend aditivo cada nivel apagaria o anterior e sobraria so o mais
        // largo, que sozinho e uma mancha sem nucleo.
        D3D11_BLEND_DESC additive_description = {};
        additive_description.RenderTarget[0].BlendEnable = TRUE;
        additive_description.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        additive_description.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        additive_description.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        additive_description.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        additive_description.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        additive_description.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        additive_description.RenderTarget[0].RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_ALL;
        result = device_->CreateBlendState(
            &additive_description, &additive_blend_state_);
        if (FAILED(result)) {
            log_message(
                "Falha ao criar blend aditivo do bloom: 0x%08X; "
                "o modulo fica indisponivel.",
                static_cast<unsigned>(result));
            safe_release(additive_blend_state_);
        }

        initialize_gpu_timing();

        return compile_shaders();
    }

    bool compile_shaders() {
        const CompileFromFileFunction compile_from_file =
            resolve_shader_compiler();
        if (compile_from_file == nullptr) {
            log_message("D3DCompileFromFile nao esta disponivel.");
            return false;
        }

        const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
        ID3DBlob* vertex_blob = nullptr;
        ID3DBlob* pixel_blob = nullptr;
        ID3DBlob* depth_preview_blob = nullptr;
        ID3DBlob* ssao_blob = nullptr;
        ID3DBlob* temporal_blob = nullptr;
        ID3DBlob* bloom_blobs[kBloomPassCount] = {};
        ID3DBlob* errors = nullptr;

        HRESULT result = compile_from_file(
            shader_path(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "VSMain",
            "vs_5_0",
            flags,
            0,
            &vertex_blob,
            &errors);
        if (FAILED(result)) {
            log_compile_error("vertex", result, errors);
            safe_release(errors);
            safe_release(vertex_blob);
            return false;
        }
        safe_release(errors);

        result = compile_from_file(
            shader_path(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "PSMain",
            "ps_5_0",
            flags,
            0,
            &pixel_blob,
            &errors);
        if (FAILED(result)) {
            log_compile_error("pixel", result, errors);
            safe_release(errors);
            safe_release(vertex_blob);
            safe_release(pixel_blob);
            return false;
        }
        safe_release(errors);

        const HRESULT depth_compile_result = compile_from_file(
            depth_preview_shader_path(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "PSDepthPreview",
            "ps_5_0",
            flags,
            0,
            &depth_preview_blob,
            &errors);
        if (FAILED(depth_compile_result)) {
            log_compile_error("depth preview", depth_compile_result, errors);
            safe_release(depth_preview_blob);
        }
        safe_release(errors);

        const HRESULT ssao_compile_result = compile_from_file(
            ssao_shader_path(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "PSSSAO",
            "ps_5_0",
            flags,
            0,
            &ssao_blob,
            &errors);
        if (FAILED(ssao_compile_result)) {
            log_compile_error("SSAO", ssao_compile_result, errors);
            safe_release(ssao_blob);
        }
        safe_release(errors);

        const HRESULT temporal_compile_result = compile_from_file(
            temporal_shader_path(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "PSTemporal",
            "ps_5_0",
            flags,
            0,
            &temporal_blob,
            &errors);
        if (FAILED(temporal_compile_result)) {
            log_compile_error("temporal", temporal_compile_result, errors);
            safe_release(temporal_blob);
        }
        safe_release(errors);

        // Os tres passes do bloom saem do mesmo arquivo e diferem apenas
        // pelo entry point. Escrever tres blocos identicos como os de cima
        // seria repetir quarenta linhas para trocar uma string, entao aqui a
        // tabela faz o papel.
        for (UINT index = 0; index < kBloomPassCount; ++index) {
            const HRESULT bloom_compile_result = compile_from_file(
                bloom_shader_path(),
                nullptr,
                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                kBloomEntryPoints[index],
                "ps_5_0",
                flags,
                0,
                &bloom_blobs[index],
                &errors);
            if (FAILED(bloom_compile_result)) {
                log_compile_error(
                    kBloomEntryPoints[index], bloom_compile_result, errors);
                safe_release(bloom_blobs[index]);
            }
            safe_release(errors);
        }

        ID3D11VertexShader* new_vertex_shader = nullptr;
        ID3D11PixelShader* new_pixel_shader = nullptr;
        ID3D11PixelShader* new_depth_preview_shader = nullptr;
        ID3D11PixelShader* new_ssao_shader = nullptr;
        ID3D11PixelShader* new_temporal_shader = nullptr;
        result = device_->CreateVertexShader(
            vertex_blob->GetBufferPointer(),
            vertex_blob->GetBufferSize(),
            nullptr,
            &new_vertex_shader);
        if (SUCCEEDED(result)) {
            result = device_->CreatePixelShader(
                pixel_blob->GetBufferPointer(),
                pixel_blob->GetBufferSize(),
                nullptr,
                &new_pixel_shader);
        }

        if (depth_preview_blob != nullptr) {
            const HRESULT depth_create_result = device_->CreatePixelShader(
                depth_preview_blob->GetBufferPointer(),
                depth_preview_blob->GetBufferSize(),
                nullptr,
                &new_depth_preview_shader);
            if (FAILED(depth_create_result)) {
                log_message(
                    "Falha ao criar shader de preview depth: 0x%08X.",
                    static_cast<unsigned>(depth_create_result));
                safe_release(new_depth_preview_shader);
            }
        }

        if (ssao_blob != nullptr) {
            const HRESULT ssao_create_result = device_->CreatePixelShader(
                ssao_blob->GetBufferPointer(),
                ssao_blob->GetBufferSize(),
                nullptr,
                &new_ssao_shader);
            if (FAILED(ssao_create_result)) {
                log_message(
                    "Falha ao criar shader SSAO: 0x%08X.",
                    static_cast<unsigned>(ssao_create_result));
                safe_release(new_ssao_shader);
            }
        }

        if (temporal_blob != nullptr) {
            const HRESULT temporal_create_result = device_->CreatePixelShader(
                temporal_blob->GetBufferPointer(),
                temporal_blob->GetBufferSize(),
                nullptr,
                &new_temporal_shader);
            if (FAILED(temporal_create_result)) {
                log_message(
                    "Falha ao criar shader temporal: 0x%08X.",
                    static_cast<unsigned>(temporal_create_result));
                safe_release(new_temporal_shader);
            }
        }

        ID3D11PixelShader* new_bloom_shaders[kBloomPassCount] = {};
        for (UINT index = 0; index < kBloomPassCount; ++index) {
            if (bloom_blobs[index] == nullptr) {
                continue;
            }
            const HRESULT bloom_create_result = device_->CreatePixelShader(
                bloom_blobs[index]->GetBufferPointer(),
                bloom_blobs[index]->GetBufferSize(),
                nullptr,
                &new_bloom_shaders[index]);
            if (FAILED(bloom_create_result)) {
                log_message(
                    "Falha ao criar shader %s do bloom: 0x%08X.",
                    kBloomEntryPoints[index],
                    static_cast<unsigned>(bloom_create_result));
                safe_release(new_bloom_shaders[index]);
            }
        }

        safe_release(vertex_blob);
        safe_release(pixel_blob);
        safe_release(depth_preview_blob);
        safe_release(ssao_blob);
        safe_release(temporal_blob);
        for (UINT index = 0; index < kBloomPassCount; ++index) {
            safe_release(bloom_blobs[index]);
        }
        if (FAILED(result)) {
            log_message("Falha ao criar shaders D3D11: 0x%08X.", static_cast<unsigned>(result));
            safe_release(new_vertex_shader);
            safe_release(new_pixel_shader);
            safe_release(new_depth_preview_shader);
            safe_release(new_ssao_shader);
            safe_release(new_temporal_shader);
            for (UINT index = 0; index < kBloomPassCount; ++index) {
                safe_release(new_bloom_shaders[index]);
            }
            return false;
        }

        safe_release(vertex_shader_);
        safe_release(pixel_shader_);
        vertex_shader_ = new_vertex_shader;
        pixel_shader_ = new_pixel_shader;
        if (new_depth_preview_shader != nullptr) {
            safe_release(depth_preview_shader_);
            depth_preview_shader_ = new_depth_preview_shader;
        }
        if (new_ssao_shader != nullptr) {
            safe_release(ssao_shader_);
            ssao_shader_ = new_ssao_shader;
        }
        if (new_temporal_shader != nullptr) {
            safe_release(temporal_shader_);
            temporal_shader_ = new_temporal_shader;
        }

        // O bloom so entra INTEIRO. Com um dos tres passes faltando a
        // piramide fica sem um elo -- brilho limiarizado que nunca espalha, ou
        // niveis que nunca sobem -- e o resultado e pior que nao ter bloom.
        const bool bloom_complete =
            new_bloom_shaders[0] != nullptr &&
            new_bloom_shaders[1] != nullptr &&
            new_bloom_shaders[2] != nullptr;
        if (bloom_complete) {
            safe_release(bloom_bright_shader_);
            safe_release(bloom_downsample_shader_);
            safe_release(bloom_upsample_shader_);
            bloom_bright_shader_ = new_bloom_shaders[0];
            bloom_downsample_shader_ = new_bloom_shaders[1];
            bloom_upsample_shader_ = new_bloom_shaders[2];
        } else {
            for (UINT index = 0; index < kBloomPassCount; ++index) {
                safe_release(new_bloom_shaders[index]);
            }
        }
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

    bool ensure_frame_resources(const D3D11_TEXTURE2D_DESC& source) {
        if (scene_texture_ != nullptr && width_ == source.Width &&
            height_ == source.Height && format_ == source.Format) {
            return true;
        }

        safe_release(scene_view_);
        safe_release(scene_texture_);
        safe_release(visual_target_);
        safe_release(visual_view_);
        safe_release(visual_texture_);
        safe_release(spatial_target_);
        safe_release(spatial_view_);
        safe_release(spatial_texture_);
        release_bloom_resources();
        release_temporal_resources();

        D3D11_TEXTURE2D_DESC description = source;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        description.CPUAccessFlags = 0;
        description.MiscFlags = 0;
        description.Format = typeless_format(source.Format);
        HRESULT result = device_->CreateTexture2D(
            &description, nullptr, &scene_texture_);
        if (SUCCEEDED(result) && scene_texture_ != nullptr) {
            D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
            view_description.Format = srgb_view_format(source.Format);
            view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            view_description.Texture2D.MostDetailedMip = 0;
            view_description.Texture2D.MipLevels = source.MipLevels;
            result = device_->CreateShaderResourceView(
                scene_texture_, &view_description, &scene_view_);
        }

        scene_needs_srgb_decode_ = false;
        if (FAILED(result) || scene_texture_ == nullptr || scene_view_ == nullptr) {
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
        if (FAILED(result) || scene_texture_ == nullptr || scene_view_ == nullptr) {
            log_message(
                "Falha ao criar recursos intermediarios: 0x%08X.",
                static_cast<unsigned>(result));
            safe_release(scene_view_);
            safe_release(scene_texture_);
            return false;
        }

        D3D11_TEXTURE2D_DESC visual_description = source;
        visual_description.Usage = D3D11_USAGE_DEFAULT;
        visual_description.BindFlags =
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        visual_description.CPUAccessFlags = 0;
        visual_description.MiscFlags = 0;
        visual_description.Format = typeless_format(source.Format);
        result = device_->CreateTexture2D(
            &visual_description, nullptr, &visual_texture_);
        if (SUCCEEDED(result) && visual_texture_ != nullptr) {
            D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
            view_description.Format = srgb_view_format(source.Format);
            view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            view_description.Texture2D.MostDetailedMip = 0;
            view_description.Texture2D.MipLevels = source.MipLevels;
            result = device_->CreateShaderResourceView(
                visual_texture_, &view_description, &visual_view_);
        }
        if (SUCCEEDED(result) && visual_texture_ != nullptr) {
            D3D11_RENDER_TARGET_VIEW_DESC target_description = {};
            target_description.Format = srgb_view_format(source.Format);
            target_description.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            target_description.Texture2D.MipSlice = 0;
            result = device_->CreateRenderTargetView(
                visual_texture_, &target_description, &visual_target_);
        }
        if (FAILED(result) || visual_texture_ == nullptr ||
            visual_view_ == nullptr || visual_target_ == nullptr) {
            if (!ssao_resources_failure_logged_) {
                log_message(
                    "SSAO 0.9.1 sem textura intermediaria: 0x%08X; "
                    "mantendo o passe visual normal.",
                    static_cast<unsigned>(result));
                ssao_resources_failure_logged_ = true;
            }
            safe_release(visual_target_);
            safe_release(visual_view_);
            safe_release(visual_texture_);
        }

        if (visual_target_ != nullptr) {
            result = device_->CreateTexture2D(
                &visual_description, nullptr, &spatial_texture_);
            if (SUCCEEDED(result) && spatial_texture_ != nullptr) {
                D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
                view_description.Format = srgb_view_format(source.Format);
                view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                view_description.Texture2D.MostDetailedMip = 0;
                view_description.Texture2D.MipLevels = source.MipLevels;
                result = device_->CreateShaderResourceView(
                    spatial_texture_, &view_description, &spatial_view_);
            }
            if (SUCCEEDED(result) && spatial_texture_ != nullptr) {
                D3D11_RENDER_TARGET_VIEW_DESC target_description = {};
                target_description.Format = srgb_view_format(source.Format);
                target_description.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                target_description.Texture2D.MipSlice = 0;
                result = device_->CreateRenderTargetView(
                    spatial_texture_, &target_description, &spatial_target_);
            }
            if (FAILED(result) || spatial_texture_ == nullptr ||
                spatial_view_ == nullptr || spatial_target_ == nullptr) {
                if (!temporal_resources_failure_logged_) {
                    log_message(
                        "Temporal 0.10.0 sem textura espacial: 0x%08X; "
                        "mantendo a pilha visual/SSAO anterior.",
                        static_cast<unsigned>(result));
                    temporal_resources_failure_logged_ = true;
                }
                safe_release(spatial_target_);
                safe_release(spatial_view_);
                safe_release(spatial_texture_);
            }
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

    // Quantos niveis a piramide precisa para o raio pedido.
    //
    // O alcance do glow e a resolucao do nivel mais grosseiro: com L niveis a
    // altura dele e height>>L, e o tent da subida espalha cerca de um texel e
    // meio dele. Invertendo, L = log2(raio * altura / 1.5).
    //
    // Isto e o que faz o `radius` do cfg valer em qualquer resolucao: o mesmo
    // 0.05 pede cinco niveis em 1080p e seis em 4K, e o glow ocupa a mesma
    // fracao da tela nas duas.
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

            // Abaixo de oito pixels o tent da subida amostra fora da textura
            // nos dois lados e o nivel devolve uma media chapada -- custo sem
            // alcance. A piramide para aqui, com os niveis que couberam.
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

        // Um nivel so nao e piramide: o glow fica na largura de um blur de
        // meia resolucao, uma ordem de grandeza abaixo do que as referencias
        // mostram. Melhor desligar do que entregar um halo que ninguem pediu.
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

    // Desce a piramide limiarizando, sobe somando. O resultado fica no nivel
    // 0, que o PSMain le em t1.
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

        // Descida. O nivel 0 vem da cena em resolucao cheia e ja sai
        // limiarizado; os demais so reduzem, porque quem nao passou no limiar
        // do nivel 0 nao tem como voltar.
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

        // Subida, somando. O nivel que acabou de ser lido como origem vira
        // destino na volta, entao a textura precisa sair do slot de SRV antes
        // -- caso contrario o runtime desliga a leitura em silencio e o nivel
        // some do resultado.
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
    }

    void release_frame_resources() {
        release_depth_capture_resources();
        release_bloom_resources();
        // A piramide e o staging do observador acompanham a resolucao da cena;
        // sem soltar aqui, um ResizeBuffers deixaria a amostra presa no
        // tamanho antigo e as features passariam a medir outra coisa.
        scene_observer_.release();
        safe_release(spatial_target_);
        safe_release(spatial_view_);
        safe_release(spatial_texture_);
        safe_release(visual_target_);
        safe_release(visual_view_);
        safe_release(visual_texture_);
        safe_release(scene_view_);
        safe_release(scene_texture_);
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
    // copia do depth de que as rejeicoes dependem.
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

}  // namespace

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

}  // namespace photorealism
