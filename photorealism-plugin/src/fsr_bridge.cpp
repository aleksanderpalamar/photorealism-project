#include "fsr_bridge.hpp"

#include "photorealism_fsr_api.hpp"
#include "runtime.hpp"

#include <windows.h>

#include <atomic>
#include <cwchar>

namespace photorealism {
namespace {

HMODULE g_fsr_module = nullptr;
const PhotorealismFsrApiV1* g_fsr_api = nullptr;
const PhotorealismFsrApiV2* g_fsr_api_v2 = nullptr;
const PhotorealismFsrApiV3* g_fsr_api_v3 = nullptr;
const PhotorealismFsrApiV4* g_fsr_api_v4 = nullptr;
const PhotorealismFsrApiV5* g_fsr_api_v5 = nullptr;
bool g_load_attempted = false;
std::atomic<bool> g_device_initialized{false};
std::atomic<bool> g_processing_enabled{true};

bool append_path(wchar_t* destination, size_t capacity, const wchar_t* suffix) {
    const size_t used = std::wcslen(destination);
    const size_t extra = std::wcslen(suffix);
    if (used + extra + 1 > capacity) {
        return false;
    }
    std::wmemcpy(destination + used, suffix, extra + 1);
    return true;
}

bool valid_v1_api(const PhotorealismFsrApiV1* api, std::uint32_t abi) {
    return api != nullptr && api->struct_size >= sizeof(PhotorealismFsrApiV1) &&
           api->abi_version == abi && api->initialize_device != nullptr &&
           api->shutdown_device != nullptr;
}

bool load_fsr_module() {
    if (g_load_attempted) {
        return g_fsr_api != nullptr;
    }
    g_load_attempted = true;

    wchar_t module_path[MAX_PATH] = {};
    std::wcsncpy(module_path, module_directory(), MAX_PATH - 1);
    if (!append_path(module_path, MAX_PATH, L"\\photorealism-fsr.dll")) {
        log_message(
            "Photorealism FSR/AA 0.7.0: caminho do modulo excede MAX_PATH; "
            "integracao opcional desativada.");
        return false;
    }

    g_fsr_module = LoadLibraryW(module_path);
    if (g_fsr_module == nullptr) {
        log_message(
            "Photorealism FSR/AA 0.7.0: modulo auxiliar ausente ou "
            "indisponivel (erro=%lu); nucleo 0.11.0 continua normalmente.",
            GetLastError());
        return false;
    }

    const auto get_api = reinterpret_cast<PhotorealismFsrGetApiFunction>(
        GetProcAddress(g_fsr_module, "PhotorealismFsrGetApi"));
    if (get_api == nullptr) {
        log_message(
            "Photorealism FSR/AA 0.7.0: export PhotorealismFsrGetApi ausente; "
            "modulo ignorado sem alterar o nucleo.");
        FreeLibrary(g_fsr_module);
        g_fsr_module = nullptr;
        return false;
    }

    const PhotorealismFsrApiV1* api = nullptr;
    HRESULT result = get_api(PHOTOREALISM_FSR_ABI_V5, &api);
    if (SUCCEEDED(result) && valid_v1_api(api, PHOTOREALISM_FSR_ABI_V5) &&
        api->struct_size >= sizeof(PhotorealismFsrApiV5)) {
        const auto* api_v5 = reinterpret_cast<const PhotorealismFsrApiV5*>(api);
        const auto* api_v4 = &api_v5->base;
        const auto* api_v3 = &api_v4->base;
        const auto* api_v2 = &api_v3->base;
        if (api_v2->observe_color_targets != nullptr &&
            api_v2->observe_frame != nullptr &&
            api_v2->reset_color_observation != nullptr &&
            api_v3->update_automatic_selection != nullptr &&
            api_v4->process_shader_resources != nullptr &&
            api_v5->observe_pixel_shader_resources != nullptr &&
            api_v5->observe_final_draw != nullptr) {
            g_fsr_api = api;
            g_fsr_api_v2 = api_v2;
            g_fsr_api_v3 = api_v3;
            g_fsr_api_v4 = api_v4;
            g_fsr_api_v5 = api_v5;
        }
    }

    if (g_fsr_api == nullptr) {
        api = nullptr;
        result = get_api(PHOTOREALISM_FSR_ABI_V4, &api);
    }
    if (g_fsr_api == nullptr && SUCCEEDED(result) &&
        valid_v1_api(api, PHOTOREALISM_FSR_ABI_V4) &&
        api->struct_size >= sizeof(PhotorealismFsrApiV4)) {
        const auto* api_v4 = reinterpret_cast<const PhotorealismFsrApiV4*>(api);
        const auto* api_v3 = &api_v4->base;
        const auto* api_v2 = &api_v3->base;
        if (api_v2->observe_color_targets != nullptr &&
            api_v2->observe_frame != nullptr &&
            api_v2->reset_color_observation != nullptr &&
            api_v3->update_automatic_selection != nullptr &&
            api_v4->process_shader_resources != nullptr) {
            g_fsr_api = api;
            g_fsr_api_v2 = api_v2;
            g_fsr_api_v3 = api_v3;
            g_fsr_api_v4 = api_v4;
        }
    }

    if (g_fsr_api == nullptr) {
        api = nullptr;
        result = get_api(PHOTOREALISM_FSR_ABI_V3, &api);
    }
    if (g_fsr_api == nullptr && SUCCEEDED(result) &&
        valid_v1_api(api, PHOTOREALISM_FSR_ABI_V3) &&
        api->struct_size >= sizeof(PhotorealismFsrApiV3)) {
        const auto* api_v3 = reinterpret_cast<const PhotorealismFsrApiV3*>(api);
        const auto* api_v2 = &api_v3->base;
        if (api_v2->observe_color_targets != nullptr &&
            api_v2->observe_frame != nullptr &&
            api_v2->reset_color_observation != nullptr &&
            api_v3->update_automatic_selection != nullptr) {
            g_fsr_api = api;
            g_fsr_api_v2 = api_v2;
            g_fsr_api_v3 = api_v3;
        }
    }

    if (g_fsr_api == nullptr) {
        api = nullptr;
        result = get_api(PHOTOREALISM_FSR_ABI_V2, &api);
    }
    if (g_fsr_api == nullptr && SUCCEEDED(result) &&
        valid_v1_api(api, PHOTOREALISM_FSR_ABI_V2) &&
        api->struct_size >= sizeof(PhotorealismFsrApiV2)) {
        const auto* api_v2 = reinterpret_cast<const PhotorealismFsrApiV2*>(api);
        if (api_v2->observe_color_targets != nullptr &&
            api_v2->observe_frame != nullptr &&
            api_v2->reset_color_observation != nullptr) {
            g_fsr_api = api;
            g_fsr_api_v2 = api_v2;
        }
    }

    if (g_fsr_api == nullptr) {
        api = nullptr;
        result = get_api(PHOTOREALISM_FSR_ABI_V1, &api);
        if (SUCCEEDED(result) && valid_v1_api(api, PHOTOREALISM_FSR_ABI_V1)) {
            g_fsr_api = api;
        }
    }

    if (g_fsr_api == nullptr) {
        log_message(
            "Photorealism FSR/AA 0.7.0: ABI incompativel ou incompleta "
            "(result=0x%08X); modulo ignorado.",
            static_cast<unsigned>(result));
        FreeLibrary(g_fsr_module);
        g_fsr_module = nullptr;
        return false;
    }

    log_message(
        "Photorealism FSR/AA 0.7.0: modulo auxiliar carregado; "
        "abi=0x%08X module=0x%08X color_observer=%s automatic_selection=%s "
        "aa_easu_rcas=%s draw_proof=%s.",
        g_fsr_api->abi_version,
        g_fsr_api->module_version,
        g_fsr_api_v2 != nullptr ? "v2-ativo" : "indisponivel-v1",
        g_fsr_api_v3 != nullptr ? "v3-ativo" : "indisponivel",
        g_fsr_api_v4 != nullptr ? "v4-ativo" : "indisponivel",
        g_fsr_api_v5 != nullptr ? "v5-ativo" : "indisponivel");
    return true;
}

}  // namespace

void initialize_fsr_module(ID3D11Device* device) {
    if (device == nullptr || !load_fsr_module() ||
        g_device_initialized.load(std::memory_order_acquire)) {
        return;
    }

    const HRESULT result = g_fsr_api->initialize_device(device);
    if (SUCCEEDED(result)) {
        g_device_initialized.store(true, std::memory_order_release);
        log_message(
            "Photorealism FSR/AA 0.7.0: dispositivo real do jogo entregue "
            "ao modulo (result=0x%08X).",
            static_cast<unsigned>(result));
    } else {
        log_message(
            "Photorealism FSR/AA 0.7.0: inicializacao do dispositivo falhou "
            "(result=0x%08X); nucleo 0.11.0 permanece ativo.",
            static_cast<unsigned>(result));
    }
}

void shutdown_fsr_device() {
    if (!g_device_initialized.exchange(false, std::memory_order_acq_rel) ||
        g_fsr_api == nullptr) {
        return;
    }
    g_fsr_api->shutdown_device();
    log_message(
        "Photorealism FSR/AA 0.7.0: dispositivo encerrado para reinicializacao "
        "segura.");
}

void observe_fsr_color_targets(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    bool includes_uavs) {
    if (!g_device_initialized.load(std::memory_order_acquire) ||
        g_fsr_api_v2 == nullptr ||
        render_target_count == D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL ||
        (render_target_count != 0 && render_targets == nullptr)) {
        return;
    }
    PhotorealismFsrColorTargetsEventV4 event = {};
    event.base.struct_size = sizeof(event);
    event.base.flags = includes_uavs
                      ? PHOTOREALISM_FSR_COLOR_EVENT_OM_SET_RENDER_TARGETS_AND_UAVS
                      : PHOTOREALISM_FSR_COLOR_EVENT_OM_SET_RENDER_TARGETS;
    event.base.render_target_count = render_target_count;
    event.base.render_targets = render_targets;
    event.context = context;
    g_fsr_api_v2->observe_color_targets(&event.base);
}

void report_fsr_frame(
    ID3D11Texture2D* back_buffer,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT sample_count) {
    if (!g_device_initialized.load(std::memory_order_acquire) ||
        g_fsr_api_v2 == nullptr || back_buffer == nullptr) {
        return;
    }
    PhotorealismFsrFrameEventV2 event = {};
    event.struct_size = sizeof(event);
    event.width = width;
    event.height = height;
    event.format = static_cast<std::uint32_t>(format);
    event.sample_count = sample_count;
    event.back_buffer = back_buffer;
    g_fsr_api_v2->observe_frame(&event);
}

void reset_fsr_color_observation_for_resize() {
    if (!g_device_initialized.load(std::memory_order_acquire) ||
        g_fsr_api_v2 == nullptr) {
        return;
    }
    g_fsr_api_v2->reset_color_observation(PHOTOREALISM_FSR_RESET_RESIZE);
}

void set_fsr_processing_enabled(bool enabled) {
    const bool previous =
        g_processing_enabled.exchange(enabled, std::memory_order_acq_rel);
    if (previous && !enabled &&
        g_device_initialized.load(std::memory_order_acquire) &&
        g_fsr_api_v2 != nullptr) {
        g_fsr_api_v2->reset_color_observation(
            PHOTOREALISM_FSR_RESET_PLUGIN_DISABLED);
    }
}

bool fsr_processing_enabled() {
    return g_processing_enabled.load(std::memory_order_acquire);
}

void report_fsr_automatic_selection_context(
    UINT backbuffer_width,
    UINT backbuffer_height,
    DXGI_FORMAT backbuffer_format,
    UINT depth_width,
    UINT depth_height,
    std::uint64_t depth_generation) {
    if (!g_device_initialized.load(std::memory_order_acquire) ||
        g_fsr_api_v3 == nullptr || backbuffer_width == 0 ||
        backbuffer_height == 0) {
        return;
    }
    PhotorealismFsrSelectionContextV3 context = {};
    context.struct_size = sizeof(context);
    context.backbuffer_width = backbuffer_width;
    context.backbuffer_height = backbuffer_height;
    context.backbuffer_format =
        static_cast<std::uint32_t>(backbuffer_format);
    context.depth_width = depth_width;
    context.depth_height = depth_height;
    context.depth_generation = depth_generation;
    PhotorealismFsrAutomaticSelectionV3 selection = {};
    selection.struct_size = sizeof(selection);
    g_fsr_api_v3->update_automatic_selection(&context, &selection);
}

void observe_fsr_pixel_shader_resources(
    ID3D11DeviceContext* context,
    UINT start_slot,
    UINT view_count,
    ID3D11ShaderResourceView* const* views) {
    if (!g_device_initialized.load(std::memory_order_acquire) ||
        g_fsr_api_v5 == nullptr || context == nullptr || view_count == 0 ||
        views == nullptr) {
        return;
    }
    PhotorealismFsrPixelShaderResourcesEventV5 event = {};
    event.struct_size = sizeof(event);
    event.start_slot = start_slot;
    event.view_count = view_count;
    event.context = context;
    event.views = views;
    g_fsr_api_v5->observe_pixel_shader_resources(&event);
}

void observe_fsr_final_draw(
    ID3D11DeviceContext* context,
    std::uint32_t kind,
    UINT primitive_count,
    UINT instance_count,
    UINT start_location,
    INT base_vertex_location,
    UINT start_instance_location) {
    if (!g_device_initialized.load(std::memory_order_acquire) ||
        g_fsr_api_v5 == nullptr || context == nullptr) {
        return;
    }
    PhotorealismFsrDrawEventV5 event = {};
    event.struct_size = sizeof(event);
    event.kind = kind;
    event.primitive_count = primitive_count;
    event.instance_count = instance_count;
    event.start_location = start_location;
    event.base_vertex_location = base_vertex_location;
    event.start_instance_location = start_instance_location;
    event.context = context;
    g_fsr_api_v5->observe_final_draw(&event);
}

}  // namespace photorealism
