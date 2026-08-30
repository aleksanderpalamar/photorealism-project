#include "postprocess.hpp"

#include "config.hpp"
#include "fsr_bridge.hpp"
#include "resource_observer.hpp"
#include "runtime.hpp"
#include "steam_screenshots.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <windows.h>

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
};

static_assert(sizeof(ShaderConstants) == 64, "constant buffer must be aligned");

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

struct RtgiConstants {
    float depth_texel_size[2];
    float projection_scale[2];
    float near_plane;
    float ray_count;
    float max_steps;
    float range_min;
    float range_max;
    float gi_intensity;
    float max_indirect_luma;
    float sky_ambient;
    float hit_thickness;
    float normal_bias;
    float debug_mode;
    float output_needs_srgb_encode;
    float input_needs_srgb_decode;
    float frame_index;
    float rtgi_texel_size[2];
    float history_weight;
    float depth_rejection;
    float normal_rejection;
    float color_rejection;
    float history_valid;
    float padding[3];
};

static_assert(
    sizeof(RtgiConstants) == 112,
    "RTGI constant buffer must be aligned");

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
            if (device_ != nullptr) {
                compile_shaders();
            }
            // O cfg recarregado pode ter mudado alcance, numero de raios ou
            // debug, e a historia acumulada foi tracada com os valores
            // antigos. Somar as duas coisas seria misturar duas calibracoes.
            invalidate_rtgi_history("cfg recarregado pelo End");
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
                mode_name = rtgi::rtgi_debug_mode_name(rtgi_preview_debug_);
            }
            log_message(
                "Preview depth solicitado pelo Insert: mode=%u(%s).",
                depth_preview_mode_,
                mode_name);
        }
        // Page Down sempre cicla e sempre loga. Ate a 0.13.0 ele so tinha
        // efeito com o Insert na posicao 6, e fora dela a tecla era consumida
        // sem efeito e sem registro -- uma tecla que some sem deixar rastro e
        // indiagnosticavel. O modo do Insert decide apenas se o resultado e
        // desenhado.
        if (key_pressed_once(VK_NEXT, &page_down_key_down_)) {
            rtgi_preview_debug_ =
                rtgi::next_rtgi_preview_debug(rtgi_preview_debug_);
            log_message(
                "Preview RTGI 0.13.1 pelo Page Down: debug=%s%s.",
                rtgi::rtgi_debug_mode_name(rtgi_preview_debug_),
                depth_preview_mode_ == 6
                    ? ""
                    : "; Insert na posicao 6 para desenhar");
        }
        // Page Up existe para comparacao A/B direta do RTGI, sem sair do
        // jogo e sem editar o cfg. A recarga por End volta a valer o que o
        // arquivo diz.
        if (key_pressed_once(VK_PRIOR, &page_up_key_down_)) {
            settings_.rtgi.enabled = !settings_.rtgi.enabled;
            rtgi_active_logged_generation_ = 0;
            rtgi_wait_logged_ = false;
            if (!settings_.rtgi.enabled) {
                release_rtgi_resources();
            }
            log_message(
                "RTGI 0.13.2 %s pelo atalho Page Up.",
                settings_.rtgi.enabled ? "ativado" : "desativado");
        }
        set_fsr_processing_enabled(settings_.enabled);
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
            if (!initialize_pipeline()) {
                safe_release(frame_device);
                return;
            }
            initialize_fsr_module(device_);
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

        report_fsr_frame(
            back_buffer,
            description.Width,
            description.Height,
            description.Format,
            description.SampleDesc.Count);

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
        bool rtgi_preview_active = false;
        bool rtgi_active = false;
        bool rtgi_temporal_active = false;
        bool temporal_active = false;
        ID3D11Texture2D* depth_candidate = nullptr;
        D3D11_TEXTURE2D_DESC depth_description = {};
        std::uint64_t depth_generation = 0;
        std::uint64_t depth_binding_serial = 0;
        bool depth_candidate_invalidated = false;
        const bool depth_requested =
            settings_.ssao_enabled || settings_.temporal_enabled ||
            settings_.rtgi.enabled || depth_preview_mode_ != 0;
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

        report_fsr_automatic_selection_context(
            description.Width,
            description.Height,
            description.Format,
            depth_available ? depth_description.Width : 0u,
            depth_available ? depth_description.Height : 0u,
            depth_available ? depth_generation : 0u);

        depth_preview_active =
            depth_available && depth_preview_mode_ >= 1 &&
            depth_preview_mode_ <= 4 && depth_preview_shader_ != nullptr;
        ssao_preview_active =
            depth_available && depth_preview_mode_ == 5 &&
            ssao_shader_ != nullptr;
        rtgi_preview_active =
            depth_available && depth_preview_mode_ == 6 &&
            rtgi_shader_ != nullptr;
        // O passe de GI roda antes do grading, para que exposure, contraste e
        // LUT alcancem tanto a luz direta quanto a indireta.
        if (depth_available && depth_preview_mode_ == 0 &&
            settings_.rtgi.enabled && rtgi_shader_ != nullptr) {
            rtgi_active = ensure_rtgi_resources(description, depth_generation);
            if (rtgi_active) {
                rtgi_temporal_active =
                    ensure_rtgi_temporal_resources(depth_description);
            }
        }
        if (!rtgi_temporal_active) {
            invalidate_rtgi_history("passe de acumulacao indisponivel");
        }
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

        if (rtgi_active) {
            rtgi_wait_logged_ = false;
            if (rtgi_active_logged_generation_ != depth_generation) {
                log_message(
                    "RTGI 0.13.2 ativo: source=%ux%u rtgi=%ux%u rays=%u "
                    "steps=%u range=%.2f-%.2f thickness=%.2f intensity=%.3f "
                    "debug=%s composicao=%s generation=%llu; marcha "
                    "geometrica, o GI e somado a cena antes do grading.",
                    description.Width,
                    description.Height,
                    rtgi_width_,
                    rtgi_height_,
                    settings_.rtgi.ray_count,
                    settings_.rtgi.max_steps,
                    settings_.rtgi.range_min,
                    settings_.rtgi.range_max,
                    settings_.rtgi.hit_thickness,
                    settings_.rtgi.gi_intensity,
                    rtgi::rtgi_debug_mode_name(settings_.rtgi.debug),
                    rtgi_compose_shader_ != nullptr ? "ok" : "indisponivel",
                    static_cast<unsigned long long>(depth_generation));
                rtgi_active_logged_generation_ = depth_generation;
            }
        } else if (depth_preview_mode_ == 0 && settings_.rtgi.enabled &&
                   !rtgi_wait_logged_) {
            log_message(
                "RTGI 0.13.2 aguardando depth e recursos validos; "
                "a pilha visual permanece intacta.");
            rtgi_wait_logged_ = true;
        }

        if (!depth_preview_active) {
            context_->CopyResource(scene_texture_, back_buffer);
            // O modo 6 tem caminho proprio (rtgi_preview_active); sem
            // exclui-lo aqui, ele logava "aguardando candidato valido" mesmo
            // com o depth disponivel -- e foi essa linha que levou a leitura
            // errada de que o depth faltava no modo 6.
            if (depth_preview_mode_ != 0 && !ssao_preview_active &&
                !rtgi_preview_active && !depth_preview_wait_logged_) {
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

        RtgiConstants rtgi_constants = {};
        rtgi_constants.depth_texel_size[0] = depth_texel_x;
        rtgi_constants.depth_texel_size[1] = depth_texel_y;
        rtgi_constants.projection_scale[0] = projection_x;
        rtgi_constants.projection_scale[1] = projection_y;
        rtgi_constants.near_plane = settings_.depth_near_plane;
        rtgi_constants.ray_count =
            static_cast<float>(settings_.rtgi.ray_count);
        rtgi_constants.max_steps =
            static_cast<float>(settings_.rtgi.max_steps);
        rtgi_constants.range_min = settings_.rtgi.range_min;
        rtgi_constants.range_max = settings_.rtgi.range_max;
        rtgi_constants.gi_intensity = settings_.rtgi.gi_intensity;
        rtgi_constants.max_indirect_luma = settings_.rtgi.max_indirect_luma;
        rtgi_constants.sky_ambient = settings_.rtgi.sky_ambient;
        rtgi_constants.hit_thickness = settings_.rtgi.hit_thickness;
        rtgi_constants.normal_bias = settings_.rtgi.normal_bias;
        // Sem decodificar para linear, o bounce seria calculado sobre valores
        // sRGB e o GI sairia claro demais nas sombras.
        rtgi_constants.input_needs_srgb_decode =
            scene_needs_srgb_decode_ ? 1.0f : 0.0f;
        // No preview quem manda e o Page Down; no passe de trabalho, o cfg.
        rtgi_constants.debug_mode = rtgi_preview_active
            ? static_cast<float>(rtgi_preview_debug_)
            : static_cast<float>(settings_.rtgi.debug);
        rtgi_constants.output_needs_srgb_encode =
            rtgi_preview_active && output_needs_srgb_encode ? 1.0f : 0.0f;
        rtgi_constants.frame_index =
            static_cast<float>(rtgi_frame_index_ & 0xFFFFull);
        // Texel do buffer de GI, e nao do depth: o clamp de vizinhanca de
        // PSRtgiTemporal anda pelos vizinhos da meia resolucao.
        rtgi_constants.rtgi_texel_size[0] =
            1.0f / static_cast<float>(rtgi_width_ > 0 ? rtgi_width_ : 1u);
        rtgi_constants.rtgi_texel_size[1] =
            1.0f / static_cast<float>(rtgi_height_ > 0 ? rtgi_height_ : 1u);
        rtgi_constants.history_weight = settings_.rtgi.history_weight;
        rtgi_constants.depth_rejection = settings_.rtgi.depth_rejection;
        rtgi_constants.normal_rejection = settings_.rtgi.normal_rejection;
        rtgi_constants.color_rejection = settings_.rtgi.color_rejection;
        rtgi_constants.history_valid = rtgi_history_valid_ ? 1.0f : 0.0f;
        context_->UpdateSubresource(
            rtgi_constant_buffer_, 0, nullptr, &rtgi_constants, 0, 0);

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

        // O GI e resolvido uma vez por frame, em meia resolucao, antes de
        // qualquer coisa que escreva no backbuffer. Desde a 0.13.2 o passe de
        // composicao soma esse buffer a cor de cena, e o grading passa a ler o
        // resultado -- e por isso que exposure, contraste e LUT alcancam tanto
        // a luz direta quanto a indireta, sem que photorealism.hlsl mude.
        ID3D11ShaderResourceView* grading_source = scene_view_;
        if (rtgi_active) {
            render_rtgi_pass(rtgi_target_, rtgi_width_, rtgi_height_);
            // Sem acumulacao a composicao le a marcha crua, exatamente como na
            // 0.13.2.1. Com acumulacao ela le o resolvido, e as duas copias
            // preparam o frame seguinte.
            ID3D11ShaderResourceView* gi_source = rtgi_view_;
            if (rtgi_temporal_active && render_rtgi_temporal_pass()) {
                gi_source = rtgi_resolved_view_;
                context_->CopyResource(
                    rtgi_history_texture_, rtgi_resolved_texture_);
                context_->CopyResource(
                    rtgi_depth_history_texture_, depth_copy_texture_);
                rtgi_history_valid_ = true;
            }
            ID3D11ShaderResourceView* composed =
                render_rtgi_compose_pass(description, gi_source);
            if (composed != nullptr) {
                grading_source = composed;
            }
            context_->RSSetViewports(1, &viewport);
        }

        if (rtgi_preview_active) {
            render_rtgi_pass(output, description.Width, description.Height);
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
            context_->PSSetShaderResources(0, 1, &grading_source);
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
            context_->PSSetShaderResources(0, 1, &grading_source);
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
            context_->PSSetShaderResources(0, 1, &grading_source);
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
        reset_fsr_color_observation_for_resize();
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
        if (source == DXGI_FORMAT_D32_FLOAT_S8X24_UINT) {
            *resource_format = DXGI_FORMAT_R32G8X24_TYPELESS;
            *view_format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
            return true;
        }
        if (source == DXGI_FORMAT_D32_FLOAT) {
            *resource_format = DXGI_FORMAT_R32_TYPELESS;
            *view_format = DXGI_FORMAT_R32_FLOAT;
            return true;
        }
        if (source == DXGI_FORMAT_D24_UNORM_S8_UINT) {
            *resource_format = DXGI_FORMAT_R24G8_TYPELESS;
            *view_format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            return true;
        }
        if (source == DXGI_FORMAT_D16_UNORM) {
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

        buffer_description.ByteWidth = sizeof(RtgiConstants);
        result = device_->CreateBuffer(
            &buffer_description, nullptr, &rtgi_constant_buffer_);
        if (FAILED(result)) {
            log_message(
                "Falha ao criar constant buffer RTGI: 0x%08X.",
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
        ID3DBlob* rtgi_blob = nullptr;
        ID3DBlob* rtgi_temporal_blob = nullptr;
        ID3DBlob* rtgi_compose_blob = nullptr;
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

        const HRESULT rtgi_compile_result = compile_from_file(
            rtgi_shader_path(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "PSRtgi",
            "ps_5_0",
            flags,
            0,
            &rtgi_blob,
            &errors);
        if (FAILED(rtgi_compile_result)) {
            log_compile_error("RTGI", rtgi_compile_result, errors);
            safe_release(rtgi_blob);
        }
        safe_release(errors);

        // Segundo entry point do mesmo arquivo: a acumulacao reaproveita o
        // cbuffer e os helpers de view-space do passe de marcha.
        const HRESULT rtgi_temporal_compile_result = compile_from_file(
            rtgi_shader_path(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "PSRtgiTemporal",
            "ps_5_0",
            flags,
            0,
            &rtgi_temporal_blob,
            &errors);
        if (FAILED(rtgi_temporal_compile_result)) {
            log_compile_error(
                "acumulacao RTGI", rtgi_temporal_compile_result, errors);
            safe_release(rtgi_temporal_blob);
        }
        safe_release(errors);

        // Terceiro entry point do mesmo arquivo: a composicao reaproveita o
        // cbuffer e o tratamento de sRGB do passe de marcha.
        const HRESULT rtgi_compose_compile_result = compile_from_file(
            rtgi_shader_path(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "PSRtgiCompose",
            "ps_5_0",
            flags,
            0,
            &rtgi_compose_blob,
            &errors);
        if (FAILED(rtgi_compose_compile_result)) {
            log_compile_error(
                "composicao RTGI", rtgi_compose_compile_result, errors);
            safe_release(rtgi_compose_blob);
        }
        safe_release(errors);

        ID3D11VertexShader* new_vertex_shader = nullptr;
        ID3D11PixelShader* new_pixel_shader = nullptr;
        ID3D11PixelShader* new_depth_preview_shader = nullptr;
        ID3D11PixelShader* new_ssao_shader = nullptr;
        ID3D11PixelShader* new_temporal_shader = nullptr;
        ID3D11PixelShader* new_rtgi_shader = nullptr;
        ID3D11PixelShader* new_rtgi_temporal_shader = nullptr;
        ID3D11PixelShader* new_rtgi_compose_shader = nullptr;
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

        if (rtgi_blob != nullptr) {
            const HRESULT rtgi_create_result = device_->CreatePixelShader(
                rtgi_blob->GetBufferPointer(),
                rtgi_blob->GetBufferSize(),
                nullptr,
                &new_rtgi_shader);
            if (FAILED(rtgi_create_result)) {
                log_message(
                    "Falha ao criar shader RTGI: 0x%08X.",
                    static_cast<unsigned>(rtgi_create_result));
                safe_release(new_rtgi_shader);
            }
        }

        if (rtgi_temporal_blob != nullptr) {
            const HRESULT temporal_gi_create_result =
                device_->CreatePixelShader(
                    rtgi_temporal_blob->GetBufferPointer(),
                    rtgi_temporal_blob->GetBufferSize(),
                    nullptr,
                    &new_rtgi_temporal_shader);
            if (FAILED(temporal_gi_create_result)) {
                log_message(
                    "Falha ao criar shader de acumulacao RTGI: 0x%08X.",
                    static_cast<unsigned>(temporal_gi_create_result));
                safe_release(new_rtgi_temporal_shader);
            }
        }

        if (rtgi_compose_blob != nullptr) {
            const HRESULT compose_create_result = device_->CreatePixelShader(
                rtgi_compose_blob->GetBufferPointer(),
                rtgi_compose_blob->GetBufferSize(),
                nullptr,
                &new_rtgi_compose_shader);
            if (FAILED(compose_create_result)) {
                log_message(
                    "Falha ao criar shader de composicao RTGI: 0x%08X.",
                    static_cast<unsigned>(compose_create_result));
                safe_release(new_rtgi_compose_shader);
            }
        }

        safe_release(vertex_blob);
        safe_release(pixel_blob);
        safe_release(depth_preview_blob);
        safe_release(ssao_blob);
        safe_release(temporal_blob);
        safe_release(rtgi_blob);
        safe_release(rtgi_temporal_blob);
        safe_release(rtgi_compose_blob);
        if (FAILED(result)) {
            log_message("Falha ao criar shaders D3D11: 0x%08X.", static_cast<unsigned>(result));
            safe_release(new_vertex_shader);
            safe_release(new_pixel_shader);
            safe_release(new_depth_preview_shader);
            safe_release(new_ssao_shader);
            safe_release(new_temporal_shader);
            safe_release(new_rtgi_shader);
            safe_release(new_rtgi_temporal_shader);
            safe_release(new_rtgi_compose_shader);
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
        if (new_rtgi_shader != nullptr) {
            safe_release(rtgi_shader_);
            rtgi_shader_ = new_rtgi_shader;
            // O shader novo pode ter mudado a forma do buffer meia-resolucao.
            release_rtgi_resources();
        }
        if (new_rtgi_temporal_shader != nullptr) {
            safe_release(rtgi_temporal_shader_);
            rtgi_temporal_shader_ = new_rtgi_temporal_shader;
        }
        if (new_rtgi_compose_shader != nullptr) {
            safe_release(rtgi_compose_shader_);
            rtgi_compose_shader_ = new_rtgi_compose_shader;
        }
        invalidate_temporal_history("recompilacao de shader");
        invalidate_rtgi_history("recompilacao de shader");
        log_message(
            "Shaders Photorealism compilados: visual=ok depth_preview=%s "
            "ssao_0.9.1=%s temporal_0.10.0=%s rtgi_0.13.2=%s "
            "rtgi_acumulacao_0.13.3=%s rtgi_composicao_0.13.2=%s.",
            depth_preview_shader_ != nullptr ? "ok" : "indisponivel",
            ssao_shader_ != nullptr ? "ok" : "indisponivel",
            temporal_shader_ != nullptr ? "ok" : "indisponivel",
            rtgi_shader_ != nullptr ? "ok" : "indisponivel",
            rtgi_temporal_shader_ != nullptr ? "ok" : "indisponivel",
            rtgi_compose_shader_ != nullptr ? "ok" : "indisponivel");
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

    // Meia resolucao por padrao: o documento fixa 960x540 em Full-HD, com
    // R16G16B16A16_FLOAT porque a luz indireta precisa de faixa alem de [0,1]
    // e o canal alfa carrega a confianca.
    bool ensure_rtgi_resources(
        const D3D11_TEXTURE2D_DESC& frame_description,
        std::uint64_t generation) {
        if (device_ == nullptr || rtgi_shader_ == nullptr ||
            rtgi_constant_buffer_ == nullptr || generation == 0) {
            return false;
        }
        const rtgi::RtgiDimensions size = rtgi::rtgi_resolution(
            frame_description.Width,
            frame_description.Height,
            settings_.rtgi.resolution_scale);
        if (size.width == 0 || size.height == 0) {
            return false;
        }
        if (rtgi_texture_ != nullptr && rtgi_view_ != nullptr &&
            rtgi_target_ != nullptr && rtgi_generation_ == generation &&
            rtgi_width_ == size.width && rtgi_height_ == size.height) {
            return true;
        }

        release_rtgi_resources();

        D3D11_TEXTURE2D_DESC description = {};
        description.Width = size.width;
        description.Height = size.height;
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags =
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        HRESULT result = device_->CreateTexture2D(
            &description, nullptr, &rtgi_texture_);
        if (SUCCEEDED(result) && rtgi_texture_ != nullptr) {
            result = device_->CreateShaderResourceView(
                rtgi_texture_, nullptr, &rtgi_view_);
        }
        if (SUCCEEDED(result) && rtgi_texture_ != nullptr) {
            result = device_->CreateRenderTargetView(
                rtgi_texture_, nullptr, &rtgi_target_);
        }
        if (FAILED(result) || rtgi_texture_ == nullptr ||
            rtgi_view_ == nullptr || rtgi_target_ == nullptr) {
            if (!rtgi_resources_failure_logged_) {
                log_message(
                    "Falha ao criar recursos RTGI 0.13.2: result=0x%08X "
                    "rtgi=%ux%u; o modulo fica inativo e a pilha atual "
                    "continua intacta.",
                    static_cast<unsigned>(result),
                    size.width,
                    size.height);
                rtgi_resources_failure_logged_ = true;
            }
            release_rtgi_resources();
            return false;
        }

        rtgi_generation_ = generation;
        rtgi_width_ = size.width;
        rtgi_height_ = size.height;
        rtgi_resources_failure_logged_ = false;
        log_message(
            "Recursos RTGI 0.13.2 criados: source=%ux%u rtgi=%ux%u "
            "format=R16G16B16A16_FLOAT generation=%llu.",
            frame_description.Width,
            frame_description.Height,
            size.width,
            size.height,
            static_cast<unsigned long long>(generation));
        return true;
    }

    // Historico do GI -- 0.13.3.
    //
    // Tres recursos: o alvo onde PSRtgiTemporal escreve o acumulado, a copia
    // desse acumulado para o frame seguinte, e a copia do depth do frame
    // anterior, de que as rejeicoes de profundidade e de normal dependem.
    //
    // O depth de historia e proprio do RTGI e nao reaproveita
    // temporal_depth_history_texture_ de proposito: aquele so e atualizado
    // quando o passe temporal da 0.10.0 esta ativo, e amarrar os dois modulos
    // faria o GI acumular ou nao conforme uma chave de outra secao do cfg.
    bool ensure_rtgi_temporal_resources(
        const D3D11_TEXTURE2D_DESC& depth_description) {
        if (device_ == nullptr || rtgi_temporal_shader_ == nullptr ||
            depth_copy_texture_ == nullptr || rtgi_texture_ == nullptr) {
            return false;
        }
        if (rtgi_resolved_texture_ != nullptr &&
            rtgi_resolved_view_ != nullptr &&
            rtgi_resolved_target_ != nullptr &&
            rtgi_history_texture_ != nullptr &&
            rtgi_history_view_ != nullptr &&
            rtgi_depth_history_texture_ != nullptr &&
            rtgi_depth_history_view_ != nullptr &&
            rtgi_temporal_depth_width_ == depth_description.Width &&
            rtgi_temporal_depth_height_ == depth_description.Height) {
            return true;
        }

        release_rtgi_temporal_resources();

        // Mesma descricao do rtgi_texture_ de ensure_rtgi_resources: a
        // acumulacao acontece na resolucao em que o GI foi tracado.
        D3D11_TEXTURE2D_DESC description = {};
        description.Width = rtgi_width_;
        description.Height = rtgi_height_;
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags =
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        HRESULT result = device_->CreateTexture2D(
            &description, nullptr, &rtgi_resolved_texture_);
        if (SUCCEEDED(result) && rtgi_resolved_texture_ != nullptr) {
            result = device_->CreateShaderResourceView(
                rtgi_resolved_texture_, nullptr, &rtgi_resolved_view_);
        }
        if (SUCCEEDED(result) && rtgi_resolved_texture_ != nullptr) {
            result = device_->CreateRenderTargetView(
                rtgi_resolved_texture_, nullptr, &rtgi_resolved_target_);
        }
        if (SUCCEEDED(result)) {
            // A historia so e lida, nunca desenhada: recebe CopyResource.
            D3D11_TEXTURE2D_DESC history_description = description;
            history_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            result = device_->CreateTexture2D(
                &history_description, nullptr, &rtgi_history_texture_);
        }
        if (SUCCEEDED(result) && rtgi_history_texture_ != nullptr) {
            result = device_->CreateShaderResourceView(
                rtgi_history_texture_, nullptr, &rtgi_history_view_);
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
                &rtgi_depth_history_texture_);
            if (SUCCEEDED(result) &&
                rtgi_depth_history_texture_ != nullptr) {
                D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
                view_description.Format = depth_view_format;
                view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                view_description.Texture2D.MostDetailedMip = 0;
                view_description.Texture2D.MipLevels = 1;
                result = device_->CreateShaderResourceView(
                    rtgi_depth_history_texture_,
                    &view_description,
                    &rtgi_depth_history_view_);
            }
        } else if (SUCCEEDED(result)) {
            result = E_INVALIDARG;
        }

        if (FAILED(result) || rtgi_resolved_texture_ == nullptr ||
            rtgi_resolved_view_ == nullptr ||
            rtgi_resolved_target_ == nullptr ||
            rtgi_history_texture_ == nullptr ||
            rtgi_history_view_ == nullptr ||
            rtgi_depth_history_texture_ == nullptr ||
            rtgi_depth_history_view_ == nullptr) {
            if (!rtgi_temporal_failure_logged_) {
                log_message(
                    "Falha ao criar historico RTGI 0.13.3: result=0x%08X "
                    "gi=%ux%u depth=%ux%u; o GI continua sendo composto sem "
                    "acumulacao, como na 0.13.2.1.",
                    static_cast<unsigned>(result),
                    rtgi_width_,
                    rtgi_height_,
                    depth_description.Width,
                    depth_description.Height);
                rtgi_temporal_failure_logged_ = true;
            }
            release_rtgi_temporal_resources();
            return false;
        }

        rtgi_temporal_depth_width_ = depth_description.Width;
        rtgi_temporal_depth_height_ = depth_description.Height;
        rtgi_history_valid_ = false;
        rtgi_temporal_failure_logged_ = false;
        log_message(
            "Historico RTGI 0.13.3 criado: gi=%ux%u depth=%ux%u "
            "history_weight=%.2f depth_rejection=%.3f normal_rejection=%.2f "
            "color_rejection=%.3f.",
            rtgi_width_,
            rtgi_height_,
            depth_description.Width,
            depth_description.Height,
            settings_.rtgi.history_weight,
            settings_.rtgi.depth_rejection,
            settings_.rtgi.normal_rejection,
            settings_.rtgi.color_rejection);
        return true;
    }

    void release_rtgi_temporal_resources() {
        safe_release(rtgi_depth_history_view_);
        safe_release(rtgi_depth_history_texture_);
        safe_release(rtgi_history_view_);
        safe_release(rtgi_history_texture_);
        safe_release(rtgi_resolved_target_);
        safe_release(rtgi_resolved_view_);
        safe_release(rtgi_resolved_texture_);
        rtgi_temporal_depth_width_ = 0;
        rtgi_temporal_depth_height_ = 0;
        rtgi_history_valid_ = false;
    }

    void invalidate_rtgi_history(const char* reason) {
        if (!rtgi_history_valid_) {
            return;
        }
        rtgi_history_valid_ = false;
        log_message("Historico RTGI 0.13.3 descartado: %s.", reason);
    }

    // Alvo em resolucao cheia onde a cena entra com a luz indireta somada.
    // Espelha visual_texture_: mesmo typeless_format/srgb_view_format do
    // backbuffer, para que o grading receba exatamente o espaco de cor que ja
    // recebia -- so que com GI dentro.
    bool ensure_rtgi_composed_resources(
        const D3D11_TEXTURE2D_DESC& source) {
        if (device_ == nullptr) {
            return false;
        }
        if (rtgi_composed_texture_ != nullptr &&
            rtgi_composed_view_ != nullptr &&
            rtgi_composed_target_ != nullptr &&
            rtgi_composed_width_ == source.Width &&
            rtgi_composed_height_ == source.Height) {
            return true;
        }

        release_rtgi_composed_resources();

        D3D11_TEXTURE2D_DESC description = source;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags =
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        description.CPUAccessFlags = 0;
        description.MiscFlags = 0;
        description.Format = typeless_format(source.Format);
        HRESULT result = device_->CreateTexture2D(
            &description, nullptr, &rtgi_composed_texture_);
        if (SUCCEEDED(result) && rtgi_composed_texture_ != nullptr) {
            D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
            view_description.Format = srgb_view_format(source.Format);
            view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            view_description.Texture2D.MostDetailedMip = 0;
            view_description.Texture2D.MipLevels = source.MipLevels;
            result = device_->CreateShaderResourceView(
                rtgi_composed_texture_,
                &view_description,
                &rtgi_composed_view_);
        }
        if (SUCCEEDED(result) && rtgi_composed_texture_ != nullptr) {
            D3D11_RENDER_TARGET_VIEW_DESC target_description = {};
            target_description.Format = srgb_view_format(source.Format);
            target_description.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            result = device_->CreateRenderTargetView(
                rtgi_composed_texture_,
                &target_description,
                &rtgi_composed_target_);
        }
        if (FAILED(result) || rtgi_composed_texture_ == nullptr ||
            rtgi_composed_view_ == nullptr ||
            rtgi_composed_target_ == nullptr) {
            if (!rtgi_composed_failure_logged_) {
                log_message(
                    "Falha ao criar alvo de composicao RTGI 0.13.2: "
                    "result=0x%08X size=%ux%u; o GI fica sem compor e a pilha "
                    "atual continua intacta.",
                    static_cast<unsigned>(result),
                    source.Width,
                    source.Height);
                rtgi_composed_failure_logged_ = true;
            }
            release_rtgi_composed_resources();
            return false;
        }

        rtgi_composed_width_ = source.Width;
        rtgi_composed_height_ = source.Height;
        rtgi_composed_failure_logged_ = false;
        return true;
    }

    void release_rtgi_composed_resources() {
        safe_release(rtgi_composed_target_);
        safe_release(rtgi_composed_view_);
        safe_release(rtgi_composed_texture_);
        rtgi_composed_width_ = 0;
        rtgi_composed_height_ = 0;
    }

    void release_rtgi_resources() {
        release_rtgi_composed_resources();
        // O historico vive na resolucao do GI: se ela muda, ele nao serve.
        release_rtgi_temporal_resources();
        safe_release(rtgi_target_);
        safe_release(rtgi_view_);
        safe_release(rtgi_texture_);
        rtgi_generation_ = 0;
        rtgi_width_ = 0;
        rtgi_height_ = 0;
        rtgi_active_logged_generation_ = 0;
        rtgi_wait_logged_ = false;
    }

    // Um unico lugar que sabe desenhar o passe de GI. A cadeia principal so
    // decide para onde ele escreve, nunca como.
    void render_rtgi_pass(
        ID3D11RenderTargetView* target, UINT width, UINT height) {
        if (context_ == nullptr || target == nullptr ||
            rtgi_shader_ == nullptr || rtgi_constant_buffer_ == nullptr) {
            return;
        }
        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &viewport);
        context_->OMSetRenderTargets(1, &target, nullptr);
        context_->PSSetShader(rtgi_shader_, nullptr, 0);
        ID3D11ShaderResourceView* resources[2] = {
            scene_view_, depth_copy_view_};
        ID3D11SamplerState* samplers[2] = {
            sampler_state_, depth_sampler_state_};
        context_->PSSetShaderResources(0, 2, resources);
        context_->PSSetSamplers(0, 2, samplers);
        context_->PSSetConstantBuffers(0, 1, &rtgi_constant_buffer_);
        context_->Draw(3, 0);
        context_->OMSetRenderTargets(0, nullptr, nullptr);
        ID3D11ShaderResourceView* null_resources[2] = {};
        context_->PSSetShaderResources(0, 2, null_resources);
        ++rtgi_frame_index_;
    }

    // Irmao do render_rtgi_pass: soma o buffer de GI a cor de cena, em
    // resolucao cheia, antes do grading. Devolve a SRV que o grading deve ler,
    // ou nullptr quando a composicao nao pode acontecer -- e ai a cadeia segue
    // lendo scene_view_ e fica byte a byte a de antes da 0.13.2.
    // gi_source e rtgi_view_ quando nao ha acumulacao e rtgi_resolved_view_
    // quando ha. Passar por parametro em vez de escolher aqui dentro mantem o
    // passe de composicao sem opiniao sobre a 0.13.3.
    ID3D11ShaderResourceView* render_rtgi_compose_pass(
        const D3D11_TEXTURE2D_DESC& source,
        ID3D11ShaderResourceView* gi_source) {
        if (context_ == nullptr || rtgi_compose_shader_ == nullptr ||
            rtgi_constant_buffer_ == nullptr || gi_source == nullptr ||
            scene_view_ == nullptr) {
            return nullptr;
        }
        if (!ensure_rtgi_composed_resources(source)) {
            return nullptr;
        }
        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(source.Width);
        viewport.Height = static_cast<float>(source.Height);
        viewport.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &viewport);
        context_->OMSetRenderTargets(1, &rtgi_composed_target_, nullptr);
        context_->PSSetShader(rtgi_compose_shader_, nullptr, 0);
        ID3D11ShaderResourceView* resources[3] = {
            scene_view_, depth_copy_view_, gi_source};
        ID3D11SamplerState* samplers[2] = {
            sampler_state_, depth_sampler_state_};
        context_->PSSetShaderResources(0, 3, resources);
        context_->PSSetSamplers(0, 2, samplers);
        context_->PSSetConstantBuffers(0, 1, &rtgi_constant_buffer_);
        context_->Draw(3, 0);
        context_->OMSetRenderTargets(0, nullptr, nullptr);
        ID3D11ShaderResourceView* null_resources[3] = {};
        context_->PSSetShaderResources(0, 3, null_resources);
        return rtgi_composed_view_;
    }

    // Acumulacao temporal do GI -- 0.13.3. Roda na resolucao do RTGI, entre a
    // marcha e a composicao.
    //
    // Le o GI atual (t2), a historia (t3), o depth atual (t1) e o depth do
    // frame anterior (t4). t0 fica sem uso: PSRtgiTemporal nao toca na cor de
    // cena, so no buffer de luz indireta.
    bool render_rtgi_temporal_pass() {
        if (context_ == nullptr || rtgi_temporal_shader_ == nullptr ||
            rtgi_constant_buffer_ == nullptr || rtgi_view_ == nullptr ||
            rtgi_resolved_target_ == nullptr) {
            return false;
        }
        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(rtgi_width_);
        viewport.Height = static_cast<float>(rtgi_height_);
        viewport.MaxDepth = 1.0f;
        context_->RSSetViewports(1, &viewport);
        context_->OMSetRenderTargets(1, &rtgi_resolved_target_, nullptr);
        context_->PSSetShader(rtgi_temporal_shader_, nullptr, 0);
        ID3D11ShaderResourceView* resources[5] = {
            nullptr,
            depth_copy_view_,
            rtgi_view_,
            rtgi_history_view_,
            rtgi_depth_history_view_};
        ID3D11SamplerState* samplers[2] = {
            sampler_state_, depth_sampler_state_};
        context_->PSSetShaderResources(0, 5, resources);
        context_->PSSetSamplers(0, 2, samplers);
        context_->PSSetConstantBuffers(0, 1, &rtgi_constant_buffer_);
        context_->Draw(3, 0);
        context_->OMSetRenderTargets(0, nullptr, nullptr);
        ID3D11ShaderResourceView* null_resources[5] = {};
        context_->PSSetShaderResources(0, 5, null_resources);
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
        release_rtgi_resources();
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

    void release_frame_resources() {
        release_depth_capture_resources();
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
        rtgi_resources_failure_logged_ = false;
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
        shutdown_fsr_device();
        release_frame_resources();
        release_gpu_timing();
        safe_release(vertex_shader_);
        safe_release(pixel_shader_);
        safe_release(depth_preview_shader_);
        safe_release(ssao_shader_);
        safe_release(temporal_shader_);
        safe_release(rtgi_shader_);
        safe_release(rtgi_temporal_shader_);
        safe_release(rtgi_compose_shader_);
        safe_release(constant_buffer_);
        safe_release(depth_constant_buffer_);
        safe_release(ssao_constant_buffer_);
        safe_release(temporal_constant_buffer_);
        safe_release(rtgi_constant_buffer_);
        safe_release(sampler_state_);
        safe_release(depth_sampler_state_);
        safe_release(rasterizer_state_);
        safe_release(depth_state_);
        safe_release(blend_state_);
        safe_release(context_);
        safe_release(device_);
    }

    Settings settings_;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    ID3D11Texture2D* scene_texture_ = nullptr;
    ID3D11ShaderResourceView* scene_view_ = nullptr;
    ID3D11Texture2D* visual_texture_ = nullptr;
    ID3D11ShaderResourceView* visual_view_ = nullptr;
    ID3D11RenderTargetView* visual_target_ = nullptr;
    ID3D11Texture2D* spatial_texture_ = nullptr;
    ID3D11ShaderResourceView* spatial_view_ = nullptr;
    ID3D11RenderTargetView* spatial_target_ = nullptr;
    ID3D11Texture2D* depth_copy_texture_ = nullptr;
    ID3D11ShaderResourceView* depth_copy_view_ = nullptr;
    ID3D11Texture2D* rtgi_texture_ = nullptr;
    ID3D11ShaderResourceView* rtgi_view_ = nullptr;
    ID3D11RenderTargetView* rtgi_target_ = nullptr;
    ID3D11PixelShader* rtgi_shader_ = nullptr;
    ID3D11PixelShader* rtgi_temporal_shader_ = nullptr;
    ID3D11PixelShader* rtgi_compose_shader_ = nullptr;
    ID3D11Buffer* rtgi_constant_buffer_ = nullptr;
    std::uint64_t rtgi_generation_ = 0;
    UINT rtgi_width_ = 0;
    UINT rtgi_height_ = 0;
    std::uint64_t rtgi_frame_index_ = 0;
    bool rtgi_resources_failure_logged_ = false;
    std::uint64_t rtgi_active_logged_generation_ = 0;
    ID3D11Texture2D* rtgi_composed_texture_ = nullptr;
    ID3D11ShaderResourceView* rtgi_composed_view_ = nullptr;
    ID3D11RenderTargetView* rtgi_composed_target_ = nullptr;
    UINT rtgi_composed_width_ = 0;
    UINT rtgi_composed_height_ = 0;
    bool rtgi_composed_failure_logged_ = false;
    bool rtgi_wait_logged_ = false;
    // Historico do GI -- 0.13.3. rtgi_resolved_ e o alvo da acumulacao,
    // rtgi_history_ a copia dele para o frame seguinte, rtgi_depth_history_ a
    // copia do depth de que as rejeicoes dependem.
    ID3D11Texture2D* rtgi_resolved_texture_ = nullptr;
    ID3D11ShaderResourceView* rtgi_resolved_view_ = nullptr;
    ID3D11RenderTargetView* rtgi_resolved_target_ = nullptr;
    ID3D11Texture2D* rtgi_history_texture_ = nullptr;
    ID3D11ShaderResourceView* rtgi_history_view_ = nullptr;
    ID3D11Texture2D* rtgi_depth_history_texture_ = nullptr;
    ID3D11ShaderResourceView* rtgi_depth_history_view_ = nullptr;
    UINT rtgi_temporal_depth_width_ = 0;
    UINT rtgi_temporal_depth_height_ = 0;
    bool rtgi_history_valid_ = false;
    bool rtgi_temporal_failure_logged_ = false;
    ID3D11Texture2D* temporal_history_texture_ = nullptr;
    ID3D11ShaderResourceView* temporal_history_view_ = nullptr;
    ID3D11Texture2D* temporal_depth_history_texture_ = nullptr;
    ID3D11ShaderResourceView* temporal_depth_history_view_ = nullptr;
    ID3D11VertexShader* vertex_shader_ = nullptr;
    ID3D11PixelShader* pixel_shader_ = nullptr;
    ID3D11PixelShader* depth_preview_shader_ = nullptr;
    ID3D11PixelShader* ssao_shader_ = nullptr;
    ID3D11PixelShader* temporal_shader_ = nullptr;
    ID3D11Buffer* constant_buffer_ = nullptr;
    ID3D11Buffer* depth_constant_buffer_ = nullptr;
    ID3D11Buffer* ssao_constant_buffer_ = nullptr;
    ID3D11Buffer* temporal_constant_buffer_ = nullptr;
    ID3D11SamplerState* sampler_state_ = nullptr;
    ID3D11SamplerState* depth_sampler_state_ = nullptr;
    ID3D11RasterizerState* rasterizer_state_ = nullptr;
    ID3D11DepthStencilState* depth_state_ = nullptr;
    ID3D11BlendState* blend_state_ = nullptr;
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
    bool resize_in_progress_ = false;
    bool home_key_down_ = false;
    bool end_key_down_ = false;
    bool insert_key_down_ = false;
    bool page_up_key_down_ = false;
    bool page_down_key_down_ = false;
    rtgi::RtgiDebugMode rtgi_preview_debug_ = rtgi::RtgiDebugMode::normals;
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
