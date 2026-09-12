#include "postprocess.hpp"

#include "../config/config.hpp"
#include "../fsr/upscaler.hpp"
#include "../resource_observer/color_observation.hpp"
#include "../hooks/present_target.hpp"
#include "../overlay/overlay.hpp"
#include "../resource_observer/resource_observer.hpp"
#include "../runtime.hpp"

#include "../scene/observer.hpp"
#include "../steam/steam_screenshots.hpp"
#include "com_utils.hpp"
#include "condition_adapter.hpp"
#include "device_state.hpp"
#include "gpu_timer.hpp"
#include "pipeline_state.hpp"
#include "shader_constants.hpp"
#include "bloom_pyramid.hpp"
#include "depth_capture.hpp"
#include "depth_liveness.hpp"
#include "format_utils.hpp"
#include "frame_constants.hpp"
#include "frame_log.hpp"
#include "frame_passes.hpp"
#include "frame_resources.hpp"
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

class PostProcessor : public overlay::MenuHost {
public:
    PostProcessor() : settings_(default_settings()) {
        overlay::menu().bind(&settings_, this);
    }

    const char* upscale_status() const override {
        return fsr::upscaler().status();
    }

    void settings_changed(const overlay::SettingBinding& binding) override {
        if (overlay::binding_touches_observer(binding)) {
            apply_scene_observer_settings();
        }
        fsr::upscaler().configure(settings_);
    }

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
        fsr::upscaler().configure(settings_);
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
        log_state_.depth_preview_wait_logged = false;
        log_state_.depth_preview_logged_mode = 0;
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
        handle_menu_hotkey();
    }

    void handle_menu_hotkey() {
        const bool control_down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool pressed = key_pressed_once('P', &menu_key_down_);
        if (!control_down || !pressed) {
            return;
        }
        overlay::menu().toggle();
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
        fsr::upscaler().configure(settings_);
        overlay::menu().bind(&settings_, this);
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

        bool expired = false;
        const bool safe = depth_liveness_.is_safe_for_scene(
            depth.generation, serial, &expired);
        if (expired) {
            release_depth_capture_resources();
        }
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
            frame_resources_.visual_target() != nullptr && frame_resources_.visual_view() != nullptr;
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
            frame_resources_.spatial_target() == nullptr || frame_resources_.spatial_view() == nullptr;
        return !(ssao_active && spatial_missing);
    }

    bool plan_bloom(
        const D3D11_TEXTURE2D_DESC& description, bool bloom_preview) {
        const bool eligible =
            (depth_preview_mode_ == 0 || bloom_preview) &&
            settings_.bloom_enabled && shaders_.bloom_bright() != nullptr &&
            pipeline_.additive_blend() != nullptr;
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
            frame_resources_.visual_target() != nullptr && frame_resources_.visual_view() != nullptr;
        plan.temporal = plan_temporal(description, plan.depth, plan.ssao);

        if (!plan.temporal && temporal_.valid()) {
            invalidate_temporal_history("depth ou passe temporal indisponivel");
        }

        plan.bloom_preview =
            depth_preview_mode_ == 6 && shaders_.bloom_bright() != nullptr &&
            pipeline_.additive_blend() != nullptr;
        plan.bloom = plan_bloom(description, plan.bloom_preview);
        if (!plan.bloom) {
            plan.bloom_preview = false;
        }
        return plan;
    }






    void capture_scene_for_grade(
        const FramePlan& plan, ID3D11Texture2D* back_buffer) {
        if (plan.depth_preview) {
            return;
        }
        context_->CopyResource(frame_resources_.scene_texture(), back_buffer);

        scene_observer_.observe(
            device_, context_, frame_resources_.scene_texture());

        condition_.update(settings_, scene_observer_.latest());

        const bool diagnostic_waiting =
            depth_preview_mode_ != 0 && !plan.ssao_preview &&
            !plan.bloom_preview && !log_state_.depth_preview_wait_logged;
        if (!diagnostic_waiting) {
            return;
        }
        log_message(
            "Diagnostico depth/SSAO aguardando candidato valido; "
            "o passe visual normal permanece ativo.");
        log_state_.depth_preview_wait_logged = true;
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
            frame_resources_.scene_needs_srgb_decode() ? "sim" : "nao",
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
        FrameLogInput log_input = {};
        log_input.description = targets.description;
        log_input.depth_description = plan.depth.description;
        log_input.depth_generation = plan.depth.generation;
        log_input.depth_preview_mode = depth_preview_mode_;
        log_input.bloom_level_count = bloom_.level_count();
        log_input.bloom_active = plan.bloom;
        log_input.depth_preview = plan.depth_preview;
        log_input.ssao_preview = plan.ssao_preview;
        log_input.ssao_active = plan.ssao;
        log_input.temporal_active = plan.temporal;
        log_frame_plan(settings_, log_input, &log_state_);
        capture_scene_for_grade(plan, targets.back_buffer);
        FrameConstantsInput input = {};
        input.description = targets.description;
        input.depth_description = plan.depth.description;
        input.depth_preview_mode = depth_preview_mode_;
        input.depth_available = plan.depth.available;
        input.ssao_active = plan.ssao;
        input.ssao_preview = plan.ssao_preview;
        input.temporal_active = plan.temporal;
        input.temporal_history_valid = temporal_.valid();
        input.bloom_active = plan.bloom;
        input.scene_needs_srgb_decode =
            frame_resources_.scene_needs_srgb_decode();
        input.output_needs_srgb_encode = targets.output_needs_srgb_encode;
        input.temperature = condition_.temperature();
        input.tint = condition_.tint();
        upload_frame_constants(context_, pipeline_, settings_, input);
        FramePassScene scene = {};
        scene.context = context_;
        scene.pipeline = &pipeline_;
        scene.shaders = &shaders_;
        scene.resources = &frame_resources_;
        scene.depth = &depth_capture_;
        scene.temporal = &temporal_;
        scene.bloom = &bloom_;
        scene.output = targets.output;
        scene.back_buffer = targets.back_buffer;
        scene.description = targets.description;
        scene.depth_description = plan.depth.description;
        scene.depth_generation = plan.depth.generation;
        scene.output_needs_srgb_encode = targets.output_needs_srgb_encode;

        FramePassPlan pass_plan = {};
        pass_plan.depth_preview = plan.depth_preview;
        pass_plan.ssao_preview = plan.ssao_preview;
        pass_plan.bloom_preview = plan.bloom_preview;
        pass_plan.ssao = plan.ssao;
        pass_plan.temporal = plan.temporal;
        pass_plan.bloom = plan.bloom;

        bind_common_pipeline_state(scene);
        compose_output(scene, pass_plan);

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
        if (!ensure_device_for(swap_chain)) {
            return;
        }
        if (!settings_.enabled) {
            return;
        }

        gpu_timer_.poll();

        FrameTargets targets = {};
        if (acquire_frame_targets(swap_chain, &targets)) {
            render_frame(targets);
        }
        release_frame_targets(&targets);
    }

    bool acquire_overlay_target(
        IDXGISwapChain* swap_chain, FrameTargets* targets) {
        if (!present_back_buffer(swap_chain, &targets->back_buffer)) {
            return false;
        }
        targets->back_buffer->GetDesc(&targets->description);
        return create_output_view(targets);
    }

    void upscale_frame(IDXGISwapChain* swap_chain) {
        if (swap_chain == nullptr || resize_in_progress_) {
            return;
        }
        if (!fsr::upscaler().wants_proxy()) {
            return;
        }
        if (!ensure_device_for(swap_chain)) {
            return;
        }

        FrameTargets targets = {};
        if (!acquire_overlay_target(swap_chain, &targets)) {
            release_frame_targets(&targets);
            return;
        }

        SavedState state = {};
        capture_state(context_, &state);
        fsr::upscaler().present(
            device_,
            context_,
            targets.output,
            targets.description.Width,
            targets.description.Height,
            !targets.output_needs_srgb_encode);
        restore_state(context_, &state);
        release_frame_targets(&targets);
    }

    void draw_overlay(IDXGISwapChain* swap_chain) {
        if (swap_chain == nullptr || resize_in_progress_) {
            return;
        }
        if (!overlay::menu().visible()) {
            return;
        }
        if (!ensure_device_for(swap_chain)) {
            return;
        }

        FrameTargets targets = {};
        if (!acquire_overlay_target(swap_chain, &targets)) {
            release_frame_targets(&targets);
            return;
        }

        DXGI_SWAP_CHAIN_DESC chain = {};
        swap_chain->GetDesc(&chain);

        SavedState state = {};
        capture_state(context_, &state);
        overlay::menu().render(
            device_,
            context_,
            targets.output,
            chain.OutputWindow,
            static_cast<float>(targets.description.Width),
            static_cast<float>(targets.description.Height),
            targets.output_needs_srgb_encode);
        restore_state(context_, &state);
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
        fsr::upscaler().release();
        reset_color_discovery();
        log_message(
            "ResizeBuffers concluido: swap_chain=%p result=0x%08X.",
            static_cast<void*>(swap_chain),
            static_cast<unsigned>(result));
    }

private:







    bool ensure_frame_resources(const D3D11_TEXTURE2D_DESC& source) {
        if (frame_resources_.matches(source)) {
            return true;
        }
        bloom_.release();
        temporal_.release();
        log_state_.temporal_active_logged_generation = 0;
        log_state_.temporal_wait_logged = false;
        if (!frame_resources_.create(source)) {
            return false;
        }
        log_message(
            "Recursos de frame criados: %ux%u format=%u "
            "ssao_intermediate=%s temporal_spatial=%s.",
            source.Width,
            source.Height,
            static_cast<unsigned>(source.Format),
            frame_resources_.visual_target() != nullptr ? "ok" : "indisponivel",
            frame_resources_.spatial_target() != nullptr ? "ok" : "indisponivel");
        return true;
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
        if (!pipeline_.create(device_)) {
            return false;
        }

        if (!bloom_.attach(device_, context_)) {
            log_message("Falha ao criar constant buffer bloom.");
            return false;
        }
        frame_resources_.attach(device_);
        depth_capture_.attach(device_, context_);
        temporal_.attach(device_, context_);
        gpu_timer_.attach(device_, context_);
        return recompile_shaders();
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
        log_state_.temporal_active_logged_generation = 0;
        log_state_.temporal_wait_logged = false;
        depth_capture_.release();
        log_state_.depth_preview_logged_mode = 0;
        log_state_.depth_preview_wait_logged = false;
        log_state_.ssao_active_logged_generation = 0;
        log_state_.ssao_wait_logged = false;
        if (reset_liveness) {
            depth_liveness_.reset();
        }
    }

    void apply_scene_observer_settings() {
        scene_observer_.configure(
            settings_.scene_observer_enabled,
            static_cast<unsigned>(settings_.scene_observer_interval_frames),
            settings_.scene_observer_log_seconds);

        condition_.reset_log();
        if (!settings_.condition_adaptation_enabled) {
            condition_.reset_state();
        }
    }

    void release_frame_resources() {
        release_depth_capture_resources();
        bloom_.release();

        scene_observer_.release();
        frame_resources_.release();
        unsupported_logged_ = false;
        processed_logged_ = false;
        temporal_.set_failure_logged(false);
        bloom_.set_failure_logged(false);
        log_state_.bloom_active_logged_levels = 0;
    }








    void reset_device() {
        shutdown_steam_screenshots();
        release_frame_resources();
        gpu_timer_.release();
        shaders_.release();
        pipeline_.release();
        bloom_.release_constant_buffer();
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
    FrameResources frame_resources_;
    ConditionAdapter condition_;
    PipelineState pipeline_;
    FrameLogState log_state_ = {};
    DepthLiveness depth_liveness_;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;



    IDXGISwapChain* active_swap_chain_ = nullptr;
    bool unsupported_logged_ = false;
    bool processed_logged_ = false;
    bool output_fallback_logged_ = false;
    bool resize_in_progress_ = false;
    bool home_key_down_ = false;
    bool end_key_down_ = false;
    bool insert_key_down_ = false;
    bool menu_key_down_ = false;
    UINT depth_preview_mode_ = 0;
};

PostProcessor g_post_processor;
thread_local bool g_inside_present = false;
SRWLOCK g_processor_lock = SRWLOCK_INIT;

class ProcessorScope {
  public:
    ProcessorScope() : entered_(!g_inside_present) {
        if (!entered_) {
            return;
        }
        g_inside_present = true;
        AcquireSRWLockExclusive(&g_processor_lock);
    }

    ~ProcessorScope() {
        if (!entered_) {
            return;
        }
        ReleaseSRWLockExclusive(&g_processor_lock);
        g_inside_present = false;
    }

    ProcessorScope(const ProcessorScope&) = delete;
    ProcessorScope& operator=(const ProcessorScope&) = delete;

    bool entered() const { return entered_; }

  private:
    bool entered_;
};
}

void process_frame(IDXGISwapChain* swap_chain) {
    ProcessorScope scope;
    if (!scope.entered()) {
        return;
    }
    g_post_processor.render(swap_chain);
}

void upscale_present_frame(IDXGISwapChain* swap_chain) {
    ProcessorScope scope;
    if (!scope.entered()) {
        return;
    }
    g_post_processor.upscale_frame(swap_chain);
    end_color_frame();
}

void draw_overlay_frame(IDXGISwapChain* swap_chain) {
    ProcessorScope scope;
    if (!scope.entered()) {
        return;
    }
    g_post_processor.draw_overlay(swap_chain);
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
