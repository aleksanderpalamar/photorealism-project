#include "photorealism_fsr_api.hpp"

#include "fsr_color_scoring.hpp"
#include "fsr_runtime.hpp"

#include <dxgi.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace {

constexpr std::size_t kViewCacheCapacity = 4096;
constexpr std::size_t kViewCacheMaxProbe = 16;
constexpr std::size_t kResourceCapacity = 256;
constexpr std::size_t kBackBufferIdentityCapacity = 8;
constexpr std::size_t kMaximumReportedTargets = 32;
constexpr std::size_t kDiagnosticQueueCapacity = 2;
constexpr std::size_t kRasterizerShadowContextCapacity = 16;
constexpr std::size_t kRejectedDrawSignatureCapacity = 64;
constexpr UINT kRejectedDrawLogArraySampleCapacity = 4;
constexpr ULONGLONG kObservationWindowMilliseconds = 30000;
constexpr ULONGLONG kDrawProofReportMilliseconds = 10000;
constexpr std::uint32_t kAutomaticSelectionConfirmFrames = 12;
constexpr std::uint32_t kAutomaticSelectionLostGraceFrames = 30;
constexpr std::uint32_t kFinalDrawProofConfirmFrames = 24;
constexpr std::uint64_t kFinalDrawProofLostGraceFrames = 2;
constexpr std::size_t kPixelShaderResourceSlotCount =
    D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
// A resource bind alone does not prove that the following draw is the final
// full-screen composition. Do not replace the game's SRV until that proof is
// available; replacing it here caused invalid UI/garage compositions.
constexpr bool kSrvReplacementRequiresDrawProof = true;
// 0.7.1 records proof only. Runtime textures and compute dispatch remain held
// for a separately tested activation release.
constexpr bool kEnableFsrRuntimeAfterDrawProof = false;

struct ViewCacheEntry {
    ID3D11RenderTargetView* view = nullptr;
    std::uint16_t resource_index = 0;
    bool occupied = false;
};

struct ShaderViewCacheEntry {
    ID3D11ShaderResourceView* view = nullptr;
    void* identity = nullptr;
    UINT width = 0;
    UINT height = 0;
    UINT format = 0;
    bool occupied = false;
};

struct ColorResourceEntry {
    void* identity = nullptr;
    UINT width = 0;
    UINT height = 0;
    UINT texture_format = 0;
    UINT primary_view_format = 0;
    UINT view_format_variants = 0;
    UINT sample_count = 0;
    UINT bind_flags = 0;
    UINT misc_flags = 0;
    UINT mip_levels = 0;
    UINT array_size = 0;
    UINT views = 0;
    std::uint64_t bindings = 0;
    std::uint64_t slot_zero_bindings = 0;
    std::uint64_t first_serial = 0;
    std::uint64_t last_serial = 0;
    std::uint64_t first_frame = 0;
    std::uint64_t last_frame = 0;
    UINT slot_mask = 0;
    UINT event_flags = 0;
    bool exact_backbuffer_resource = false;
    std::uint64_t direct_composition_hits = 0;
    std::uint64_t last_composition_frame = 0;
};

struct BackBufferIdentityEntry {
    ID3D11Texture2D* interface_pointer = nullptr;
    void* identity = nullptr;
};

struct ColorReportSnapshot {
    std::array<ColorResourceEntry, kResourceCapacity> resources = {};
    std::size_t resource_count = 0;
    UINT backbuffer_width = 0;
    UINT backbuffer_height = 0;
    UINT backbuffer_format = 0;
    UINT backbuffer_samples = 0;
    std::uint64_t frames = 0;
    std::uint64_t serial = 0;
    std::uint64_t event_count = 0;
    std::uint64_t uav_event_count = 0;
    std::uint64_t slot_bindings = 0;
    std::uint64_t total_resource_bindings = 0;
    std::uint64_t view_cache_entries = 0;
    std::uint64_t view_cache_replacements = 0;
    std::uint64_t resource_overflow = 0;
    std::uint64_t unsupported_views = 0;
    std::uint64_t lock_contention_drops = 0;
    std::uint64_t async_job_drops = 0;
    std::uint64_t report_queue_drops = 0;
    ULONGLONG elapsed_milliseconds = 0;
};

enum class DiagnosticJobState : std::uint8_t {
    empty,
    filling,
    pending,
    processing,
};

enum class DiagnosticJobType : std::uint8_t {
    none,
    color_report,
    window_notice,
    reset_notice,
    automatic_selection_ready,
    automatic_selection_fallback,
    fsr_active,
    fsr_timing,
    draw_proof_report,
};

enum class AutomaticSelectionNotice : std::uint8_t {
    none,
    ready,
    fallback,
};

struct AutomaticSelectionState {
    void* identity = nullptr;
    UINT source_width = 0;
    UINT source_height = 0;
    UINT source_format = 0;
    UINT output_width = 0;
    UINT output_height = 0;
    UINT confidence = 0;
    std::uint64_t generation = 0;
    std::uint64_t last_seen_frame = 0;
    std::uint32_t confirmations = 0;
    bool ready = false;
};

struct FinalDrawProofSignature {
    void* source_identity = nullptr;
    void* backbuffer_identity = nullptr;
    ID3D11PixelShader* pixel_shader = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    std::uint32_t kind = 0;
    std::uint32_t primitive_count = 0;
    std::uint32_t instance_count = 0;
    std::uint32_t start_location = 0;
    std::int32_t base_vertex_location = 0;
    std::uint32_t start_instance_location = 0;
    UINT source_width = 0;
    UINT source_height = 0;
    UINT source_format = 0;
    UINT output_width = 0;
    UINT output_height = 0;
};

struct RasterizerShadowState {
    ID3D11DeviceContext* context = nullptr;
    std::array<D3D11_VIEWPORT,
               D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>
        viewports = {};
    std::array<D3D11_RECT,
               D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>
        scissors = {};
    UINT viewport_count = 0;
    UINT scissor_count = 0;
    std::uint64_t epoch = 0;
    bool occupied = false;
    bool rasterizer_known = false;
    bool viewport_known = false;
    bool scissor_known = false;
    bool scissor_enabled = false;
};

enum class DrawProofRejectReason : std::uint32_t {
    missing_rtv,
    multiple_rtvs,
    depth_bound,
    wrong_backbuffer,
    source_mismatch,
    source_ineligible,
    viewport,
    scissor,
    pixel_shader,
    topology,
    call_shape,
    raster_shadow,
};

struct RejectedDrawSignature {
    DrawProofRejectReason reason = DrawProofRejectReason::missing_rtv;
    void* source_identity = nullptr;
    void* rtv_identity = nullptr;
    void* depth_stencil_view = nullptr;
    ID3D11PixelShader* pixel_shader = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    std::uint32_t kind = 0;
    std::uint32_t primitive_count = 0;
    std::uint32_t instance_count = 0;
    std::uint32_t start_location = 0;
    std::int32_t base_vertex_location = 0;
    std::uint32_t start_instance_location = 0;
    UINT source_width = 0;
    UINT source_height = 0;
    UINT source_format = 0;
    UINT rtv_width = 0;
    UINT rtv_height = 0;
    UINT rtv_format = 0;
    UINT rtv_count = 0;
    UINT viewport_count = 0;
    UINT scissor_count = 0;
    bool depth_bound = false;
    bool rasterizer_known = false;
    bool viewport_known = false;
    bool scissor_known = false;
    bool scissor_enabled = false;
    std::array<D3D11_VIEWPORT,
               D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>
        viewports = {};
    std::array<D3D11_RECT,
               D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>
        scissors = {};
};

struct RejectedDrawAggregate {
    RejectedDrawSignature signature = {};
    std::uint64_t hits = 0;
    std::uint64_t first_frame = 0;
    std::uint64_t last_frame = 0;
    std::uint64_t first_draw = 0;
    std::uint64_t last_draw = 0;
    bool occupied = false;
};

struct DrawProofDiagnostics {
    std::uint64_t observed = 0;
    std::uint64_t valid = 0;
    std::uint64_t missing_shadow = 0;
    std::uint64_t wrong_context = 0;
    std::uint64_t missing_rtv = 0;
    std::uint64_t multiple_rtvs = 0;
    std::uint64_t depth_bound = 0;
    std::uint64_t wrong_backbuffer = 0;
    std::uint64_t source_mismatch = 0;
    std::uint64_t source_ineligible = 0;
    std::uint64_t viewport = 0;
    std::uint64_t scissor = 0;
    std::uint64_t pixel_shader = 0;
    std::uint64_t topology = 0;
    std::uint64_t call_shape = 0;
    std::uint64_t duplicate_frame = 0;
    std::uint64_t lock_contention = 0;
    std::uint64_t raster_shadow = 0;
    std::uint64_t rejected_signature_overflow = 0;
};

struct DrawProofReportSnapshot {
    DrawProofDiagnostics diagnostics = {};
    std::uint64_t streak = 0;
    bool locked = false;
    void* source_identity = nullptr;
    UINT source_width = 0;
    UINT source_height = 0;
    UINT output_width = 0;
    UINT output_height = 0;
    std::array<RejectedDrawAggregate, kRejectedDrawSignatureCapacity>
        rejected_draws = {};
    std::size_t rejected_draw_count = 0;
    std::uint64_t rejected_draw_overflow = 0;
};

enum class ColorReportReason : std::uint8_t {
    window_complete,
    device_shutdown,
    device_replaced,
};

struct DiagnosticJob {
    DiagnosticJobState state = DiagnosticJobState::empty;
    DiagnosticJobType type = DiagnosticJobType::none;
    ColorReportReason report_reason = ColorReportReason::window_complete;
    ColorReportSnapshot snapshot = {};
    DrawProofReportSnapshot draw_proof_report = {};
    UINT width = 0;
    UINT height = 0;
    UINT format = 0;
    UINT sample_count = 0;
    std::uint32_t reset_reason = 0;
    bool signature_changed = false;
    std::uint64_t generation = 0;
    UINT source_width = 0;
    UINT source_height = 0;
    UINT confidence = 0;
    std::uint64_t timing_samples = 0;
    std::uint64_t timing_dropped = 0;
    double easu_sum_ms = 0.0;
    double rcas_sum_ms = 0.0;
    double temporal_aa_sum_ms = 0.0;
    double easu_max_ms = 0.0;
    double rcas_max_ms = 0.0;
    double temporal_aa_max_ms = 0.0;
    bool fsr_upscale_enabled = false;
};

HMODULE g_module = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_immediate_context = nullptr;
SRWLOCK g_catalog_lock = SRWLOCK_INIT;
SRWLOCK g_runtime_lock = SRWLOCK_INIT;
SRWLOCK g_diagnostic_queue_lock = SRWLOCK_INIT;
CONDITION_VARIABLE g_diagnostic_fill_condition = CONDITION_VARIABLE_INIT;
INIT_ONCE g_log_path_once = INIT_ONCE_STATIC_INIT;
wchar_t g_log_path[MAX_PATH] = {};

std::array<ViewCacheEntry, kViewCacheCapacity> g_view_cache = {};
std::array<ShaderViewCacheEntry, kViewCacheCapacity> g_shader_view_cache = {};
std::array<ColorResourceEntry, kResourceCapacity> g_resources = {};
std::array<BackBufferIdentityEntry, kBackBufferIdentityCapacity>
    g_backbuffer_identities = {};
std::array<ID3D11ShaderResourceView*, kPixelShaderResourceSlotCount>
    g_pixel_shader_resource_shadow = {};
std::array<RasterizerShadowState, kRasterizerShadowContextCapacity>
    g_rasterizer_shadows = {};
std::array<RejectedDrawAggregate, kRejectedDrawSignatureCapacity>
    g_rejected_draws = {};
std::atomic<bool> g_pixel_shader_slot_zero_candidate{false};
std::atomic<std::uint64_t> g_rasterizer_shadow_epoch{1};
std::size_t g_resource_count = 0;
std::size_t g_backbuffer_identity_count = 0;
std::uint64_t g_view_cache_entries = 0;
std::uint64_t g_view_cache_replacements = 0;
std::uint64_t g_resource_overflow = 0;
std::uint64_t g_unsupported_views = 0;
std::uint64_t g_frame_serial = 0;
std::uint64_t g_lifetime_frame_serial = 0;
std::uint64_t g_binding_serial = 0;
std::uint64_t g_draw_proof_draw_serial = 0;
std::uint64_t g_event_count = 0;
std::uint64_t g_uav_event_count = 0;
std::uint64_t g_slot_bindings = 0;
std::uint64_t g_total_resource_bindings = 0;
volatile LONG64 g_lock_contention_drops = 0;
ULONGLONG g_window_started_at = 0;
UINT g_backbuffer_width = 0;
UINT g_backbuffer_height = 0;
UINT g_backbuffer_format = 0;
UINT g_backbuffer_samples = 0;
bool g_window_active = false;
void* g_current_output_identity = nullptr;
ID3D11DeviceContext* g_current_output_context = nullptr;
bool g_current_output_is_backbuffer = false;
bool g_fsr_execution_notice_pending = false;
std::uint64_t g_fsr_logged_generation = UINT64_MAX;
std::uint64_t g_srv_replacement_blocked_generation = UINT64_MAX;
bool g_fsr_runtime_available = false;
ULONGLONG g_last_timing_report_at = 0;
AutomaticSelectionState g_automatic_selection = {};
AutomaticSelectionNotice g_automatic_selection_notice =
    AutomaticSelectionNotice::none;
AutomaticSelectionState g_automatic_selection_notice_state = {};
FinalDrawProofSignature g_final_draw_proof_signature = {};
DrawProofDiagnostics g_draw_proof_diagnostics = {};
std::uint64_t g_final_draw_proof_streak = 0;
std::uint64_t g_final_draw_proof_last_frame = UINT64_MAX;
bool g_final_draw_proof_locked = false;
ULONGLONG g_last_draw_proof_report_at = 0;
std::size_t g_rejected_draw_count = 0;
std::uint64_t g_rejected_draw_overflow = 0;

std::array<DiagnosticJob, kDiagnosticQueueCapacity> g_diagnostic_jobs = {};
HANDLE g_diagnostic_event = nullptr;
HANDLE g_diagnostic_stop_event = nullptr;
HANDLE g_diagnostic_thread = nullptr;
bool g_diagnostic_accepting = false;
UINT g_diagnostic_active_fillers = 0;
volatile LONG64 g_async_job_drops = 0;
volatile LONG64 g_report_queue_drops = 0;

void reset_automatic_selection_locked(bool announce_fallback) {
    if (announce_fallback && g_automatic_selection.ready) {
        g_automatic_selection_notice = AutomaticSelectionNotice::fallback;
        g_automatic_selection_notice_state = g_automatic_selection;
    } else if (!announce_fallback) {
        g_automatic_selection_notice = AutomaticSelectionNotice::none;
        g_automatic_selection_notice_state = {};
    }
    const std::uint64_t next_generation =
        g_automatic_selection.generation + 1u;
    g_automatic_selection = {};
    g_automatic_selection.generation = next_generation;
}

bool append_path(wchar_t* destination, size_t capacity, const wchar_t* suffix) {
    const size_t used = std::wcslen(destination);
    const size_t extra = std::wcslen(suffix);
    if (used + extra + 1 > capacity) {
        return false;
    }
    std::wmemcpy(destination + used, suffix, extra + 1);
    return true;
}

BOOL CALLBACK initialize_log_path(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t module_path[MAX_PATH] = {};
    if (g_module == nullptr ||
        GetModuleFileNameW(g_module, module_path, MAX_PATH) == 0) {
        std::wcsncpy(module_path, L".", MAX_PATH - 1);
    } else {
        wchar_t* separator = std::wcsrchr(module_path, L'\\');
        if (separator != nullptr) {
            *separator = L'\0';
        }
    }

    if (!append_path(module_path, MAX_PATH, L"\\photorealism-plugin")) {
        return FALSE;
    }
    CreateDirectoryW(module_path, nullptr);
    std::wcsncpy(g_log_path, module_path, MAX_PATH - 1);
    return append_path(g_log_path, MAX_PATH, L"\\photorealism-fsr.log")
               ? TRUE
               : FALSE;
}

void log_message(const char* format, ...) {
    if (!InitOnceExecuteOnce(
            &g_log_path_once, initialize_log_path, nullptr, nullptr)) {
        return;
    }

    char message[2048] = {};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    SYSTEMTIME time = {};
    GetLocalTime(&time);
    char line[2304] = {};
    const int length = std::snprintf(
        line,
        sizeof(line),
        "[%02u:%02u:%02u.%03u] %s\r\n",
        static_cast<unsigned>(time.wHour),
        static_cast<unsigned>(time.wMinute),
        static_cast<unsigned>(time.wSecond),
        static_cast<unsigned>(time.wMilliseconds),
        message);
    if (length <= 0) {
        return;
    }

    OutputDebugStringA(line);
    HANDLE file = CreateFileW(
        g_log_path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    const DWORD bytes = static_cast<DWORD>(
        length < static_cast<int>(sizeof(line))
            ? length
            : static_cast<int>(sizeof(line) - 1));
    WriteFile(file, line, bytes, &written, nullptr);
    CloseHandle(file);
}

const char* draw_proof_reject_reason_name(DrawProofRejectReason reason);
const char* draw_proof_kind_name(std::uint32_t kind);

void write_draw_proof_report(const DrawProofReportSnapshot& snapshot) {
    const DrawProofDiagnostics& diagnostics = snapshot.diagnostics;
    log_message(
        "Draw proof 0.7.1: observed=%llu valid=%llu streak=%llu lock=%s "
        "source=%p %ux%u output=%ux%u rejects=[shadow=%llu context=%llu "
        "rtv=%llu multi_rtv=%llu dsv=%llu backbuffer=%llu source=%llu "
        "candidate=%llu viewport=%llu scissor=%llu shader=%llu topology=%llu "
        "shape=%llu raster_shadow=%llu duplicate=%llu contention=%llu "
        "signature_overflow=%llu]. replacement=0 dispatch=0.",
        static_cast<unsigned long long>(diagnostics.observed),
        static_cast<unsigned long long>(diagnostics.valid),
        static_cast<unsigned long long>(snapshot.streak),
        snapshot.locked ? "locked" : "awaiting",
        snapshot.source_identity,
        snapshot.source_width,
        snapshot.source_height,
        snapshot.output_width,
        snapshot.output_height,
        static_cast<unsigned long long>(diagnostics.missing_shadow),
        static_cast<unsigned long long>(diagnostics.wrong_context),
        static_cast<unsigned long long>(diagnostics.missing_rtv),
        static_cast<unsigned long long>(diagnostics.multiple_rtvs),
        static_cast<unsigned long long>(diagnostics.depth_bound),
        static_cast<unsigned long long>(diagnostics.wrong_backbuffer),
        static_cast<unsigned long long>(diagnostics.source_mismatch),
        static_cast<unsigned long long>(diagnostics.source_ineligible),
        static_cast<unsigned long long>(diagnostics.viewport),
        static_cast<unsigned long long>(diagnostics.scissor),
        static_cast<unsigned long long>(diagnostics.pixel_shader),
        static_cast<unsigned long long>(diagnostics.topology),
        static_cast<unsigned long long>(diagnostics.call_shape),
        static_cast<unsigned long long>(diagnostics.raster_shadow),
        static_cast<unsigned long long>(diagnostics.duplicate_frame),
        static_cast<unsigned long long>(diagnostics.lock_contention),
        static_cast<unsigned long long>(snapshot.rejected_draw_overflow));
    for (std::size_t index = 0; index < snapshot.rejected_draw_count; ++index) {
        const RejectedDrawAggregate& aggregate = snapshot.rejected_draws[index];
        if (!aggregate.occupied) {
            continue;
        }
        const RejectedDrawSignature& signature = aggregate.signature;
        log_message(
            "Rejected draw signature #%llu: reason=%s hits=%llu "
            "frames=%llu-%llu draws=%llu-%llu kind=%s count=%u instances=%u "
            "start=%u base_vertex=%d start_instance=%u PS=%p topology=%u "
            "SRV=%p %ux%u format=%u RTV=%p %ux%u format=%u count=%u "
            "DSV=%s(%p) raster_known=%s viewport_known=%s scissor_known=%s "
            "scissor_enable=%s viewport_count=%u scissor_count=%u.",
            static_cast<unsigned long long>(index),
            draw_proof_reject_reason_name(signature.reason),
            static_cast<unsigned long long>(aggregate.hits),
            static_cast<unsigned long long>(aggregate.first_frame),
            static_cast<unsigned long long>(aggregate.last_frame),
            static_cast<unsigned long long>(aggregate.first_draw),
            static_cast<unsigned long long>(aggregate.last_draw),
            draw_proof_kind_name(signature.kind),
            signature.primitive_count,
            signature.instance_count,
            signature.start_location,
            signature.base_vertex_location,
            signature.start_instance_location,
            signature.pixel_shader,
            static_cast<unsigned>(signature.topology),
            signature.source_identity,
            signature.source_width,
            signature.source_height,
            signature.source_format,
            signature.rtv_identity,
            signature.rtv_width,
            signature.rtv_height,
            signature.rtv_format,
            signature.rtv_count,
            signature.depth_bound ? "bound" : "none",
            signature.depth_stencil_view,
            signature.rasterizer_known ? "yes" : "no",
            signature.viewport_known ? "yes" : "no",
            signature.scissor_known ? "yes" : "no",
            signature.scissor_enabled ? "true" : "false",
            signature.viewport_count,
            signature.scissor_count);
        const UINT viewport_sample_count = std::min(
            signature.viewport_count, kRejectedDrawLogArraySampleCapacity);
        for (UINT viewport = 0; viewport < viewport_sample_count; ++viewport) {
            const D3D11_VIEWPORT& value = signature.viewports[viewport];
            log_message(
                "Rejected draw signature #%llu viewport[%u]=x=%.3f y=%.3f "
                "width=%.3f height=%.3f min_depth=%.3f max_depth=%.3f.",
                static_cast<unsigned long long>(index),
                viewport,
                value.TopLeftX,
                value.TopLeftY,
                value.Width,
                value.Height,
                value.MinDepth,
                value.MaxDepth);
        }
        if (signature.viewport_count > viewport_sample_count) {
            log_message(
                "Rejected draw signature #%llu viewport_samples=%u/%u; "
                "demais valores retidos somente para agregacao.",
                static_cast<unsigned long long>(index),
                viewport_sample_count,
                signature.viewport_count);
        }
        const UINT scissor_sample_count = std::min(
            signature.scissor_count, kRejectedDrawLogArraySampleCapacity);
        for (UINT scissor = 0; scissor < scissor_sample_count; ++scissor) {
            const D3D11_RECT& value = signature.scissors[scissor];
            log_message(
                "Rejected draw signature #%llu scissor[%u]=left=%ld top=%ld "
                "right=%ld bottom=%ld.",
                static_cast<unsigned long long>(index),
                scissor,
                static_cast<long>(value.left),
                static_cast<long>(value.top),
                static_cast<long>(value.right),
                static_cast<long>(value.bottom));
        }
        if (signature.scissor_count > scissor_sample_count) {
            log_message(
                "Rejected draw signature #%llu scissor_samples=%u/%u; "
                "demais valores retidos somente para agregacao.",
                static_cast<unsigned long long>(index),
                scissor_sample_count,
                signature.scissor_count);
        }
    }
}

const char* feature_level_name(D3D_FEATURE_LEVEL level) {
    switch (level) {
        case D3D_FEATURE_LEVEL_12_1:
            return "12_1";
        case D3D_FEATURE_LEVEL_12_0:
            return "12_0";
        case D3D_FEATURE_LEVEL_11_1:
            return "11_1";
        case D3D_FEATURE_LEVEL_11_0:
            return "11_0";
        case D3D_FEATURE_LEVEL_10_1:
            return "10_1";
        case D3D_FEATURE_LEVEL_10_0:
            return "10_0";
        case D3D_FEATURE_LEVEL_9_3:
            return "9_3";
        case D3D_FEATURE_LEVEL_9_2:
            return "9_2";
        case D3D_FEATURE_LEVEL_9_1:
            return "9_1";
        default:
            return "unknown";
    }
}

const char* format_name(UINT format) {
    switch (static_cast<DXGI_FORMAT>(format)) {
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            return "R16G16B16A16_FLOAT";
        case DXGI_FORMAT_R11G11B10_FLOAT:
            return "R11G11B10_FLOAT";
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            return "R10G10B10A2_UNORM";
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return "R8G8B8A8_UNORM";
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return "R8G8B8A8_UNORM_SRGB";
        case DXGI_FORMAT_B8G8R8A8_UNORM:
            return "B8G8R8A8_UNORM";
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            return "B8G8R8A8_UNORM_SRGB";
        default:
            return "other";
    }
}

void log_format_support(
    ID3D11Device* device, DXGI_FORMAT format, const char* name) {
    UINT support = 0;
    const HRESULT result = device->CheckFormatSupport(format, &support);
    if (FAILED(result)) {
        log_message(
            "Formato %s: consulta falhou result=0x%08X.",
            name,
            static_cast<unsigned>(result));
        return;
    }
    log_message(
        "Formato %s: texture2d=%s sample=%s render_target=%s typed_uav=%s "
        "flags=0x%08X.",
        name,
        (support & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0 ? "sim" : "nao",
        (support & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0 ? "sim" : "nao",
        (support & D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0 ? "sim" : "nao",
        (support & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) != 0
            ? "sim"
            : "nao",
        support);
}

void log_adapter(ID3D11Device* device) {
    IDXGIDevice* dxgi_device = nullptr;
    HRESULT result = device->QueryInterface(
        IID_IDXGIDevice, reinterpret_cast<void**>(&dxgi_device));
    if (FAILED(result) || dxgi_device == nullptr) {
        log_message(
            "Adaptador DXGI indisponivel: result=0x%08X.",
            static_cast<unsigned>(result));
        return;
    }

    IDXGIAdapter* adapter = nullptr;
    result = dxgi_device->GetAdapter(&adapter);
    dxgi_device->Release();
    if (FAILED(result) || adapter == nullptr) {
        log_message(
            "GetAdapter falhou: result=0x%08X.",
            static_cast<unsigned>(result));
        return;
    }

    DXGI_ADAPTER_DESC description = {};
    result = adapter->GetDesc(&description);
    adapter->Release();
    if (FAILED(result)) {
        log_message(
            "GetDesc do adaptador falhou: result=0x%08X.",
            static_cast<unsigned>(result));
        return;
    }

    char adapter_name[256] = {};
    if (WideCharToMultiByte(
            CP_UTF8,
            0,
            description.Description,
            -1,
            adapter_name,
            static_cast<int>(sizeof(adapter_name)),
            nullptr,
            nullptr) == 0) {
        std::snprintf(adapter_name, sizeof(adapter_name), "indisponivel");
    }
    log_message(
        "Adapter: name=%s vendor=0x%04X device=0x%04X subsystem=0x%08X "
        "revision=0x%X dedicated_vram=%lluMiB shared_memory=%lluMiB "
        "luid=%08lX:%08lX.",
        adapter_name,
        description.VendorId,
        description.DeviceId,
        description.SubSysId,
        description.Revision,
        static_cast<unsigned long long>(
            description.DedicatedVideoMemory / (1024ull * 1024ull)),
        static_cast<unsigned long long>(
            description.SharedSystemMemory / (1024ull * 1024ull)),
        static_cast<unsigned long>(description.AdapterLuid.HighPart),
        static_cast<unsigned long>(description.AdapterLuid.LowPart));
}

std::size_t pointer_hash(const void* pointer, std::size_t capacity) {
    std::uintptr_t value = reinterpret_cast<std::uintptr_t>(pointer);
    value >>= 4;
    value ^= value >> 17;
    value *= static_cast<std::uintptr_t>(0x9E3779B185EBCA87ull);
    return static_cast<std::size_t>(value) & (capacity - 1);
}

void* normalized_identity(IUnknown* object) {
    if (object == nullptr) {
        return nullptr;
    }
    IUnknown* identity = nullptr;
    if (FAILED(object->QueryInterface(
            IID_IUnknown, reinterpret_cast<void**>(&identity))) ||
        identity == nullptr) {
        return static_cast<void*>(object);
    }
    void* token = static_cast<void*>(identity);
    identity->Release();
    return token;
}

bool is_backbuffer_identity_locked(void* identity) {
    for (std::size_t index = 0; index < g_backbuffer_identity_count; ++index) {
        if (g_backbuffer_identities[index].identity == identity) {
            return true;
        }
    }
    return false;
}

bool final_draw_signature_matches(
    const FinalDrawProofSignature& left,
    const FinalDrawProofSignature& right) {
    return left.source_identity == right.source_identity &&
           left.backbuffer_identity == right.backbuffer_identity &&
           left.pixel_shader == right.pixel_shader &&
           left.topology == right.topology && left.kind == right.kind &&
           left.primitive_count == right.primitive_count &&
           left.instance_count == right.instance_count &&
           left.start_location == right.start_location &&
           left.base_vertex_location == right.base_vertex_location &&
           left.start_instance_location == right.start_instance_location &&
           left.source_width == right.source_width &&
           left.source_height == right.source_height &&
           left.source_format == right.source_format &&
           left.output_width == right.output_width &&
           left.output_height == right.output_height;
}

const char* draw_proof_reject_reason_name(DrawProofRejectReason reason) {
    switch (reason) {
        case DrawProofRejectReason::missing_rtv:
            return "missing-rtv";
        case DrawProofRejectReason::multiple_rtvs:
            return "multiple-rtvs";
        case DrawProofRejectReason::depth_bound:
            return "depth-bound";
        case DrawProofRejectReason::wrong_backbuffer:
            return "wrong-backbuffer";
        case DrawProofRejectReason::source_mismatch:
            return "source-mismatch";
        case DrawProofRejectReason::source_ineligible:
            return "source-ineligible";
        case DrawProofRejectReason::viewport:
            return "viewport";
        case DrawProofRejectReason::scissor:
            return "scissor";
        case DrawProofRejectReason::pixel_shader:
            return "pixel-shader";
        case DrawProofRejectReason::topology:
            return "topology";
        case DrawProofRejectReason::call_shape:
            return "call-shape";
        case DrawProofRejectReason::raster_shadow:
            return "raster-shadow";
        default:
            return "unknown";
    }
}

const char* draw_proof_kind_name(std::uint32_t kind) {
    switch (kind) {
        case PHOTOREALISM_FSR_DRAW:
            return "Draw";
        case PHOTOREALISM_FSR_DRAW_INDEXED:
            return "DrawIndexed";
        case PHOTOREALISM_FSR_DRAW_INSTANCED:
            return "DrawInstanced";
        case PHOTOREALISM_FSR_DRAW_INDEXED_INSTANCED:
            return "DrawIndexedInstanced";
        default:
            return "unknown";
    }
}

RasterizerShadowState* find_rasterizer_shadow_locked(
    ID3D11DeviceContext* context, bool create) {
    if (context == nullptr) {
        return nullptr;
    }
    const std::size_t base =
        pointer_hash(context, kRasterizerShadowContextCapacity);
    RasterizerShadowState* empty = nullptr;
    for (std::size_t probe = 0; probe < kRasterizerShadowContextCapacity;
         ++probe) {
        RasterizerShadowState& entry =
            g_rasterizer_shadows[(base + probe) &
                                 (kRasterizerShadowContextCapacity - 1)];
        if (entry.occupied && entry.context == context) {
            return &entry;
        }
        if (!entry.occupied && empty == nullptr) {
            empty = &entry;
        }
    }
    if (empty != nullptr && create) {
        *empty = {};
        empty->context = context;
        empty->occupied = true;
        return empty;
    }
    return nullptr;
}

RasterizerShadowState* prepare_rasterizer_shadow_locked(
    ID3D11DeviceContext* context) {
    RasterizerShadowState* shadow =
        find_rasterizer_shadow_locked(context, true);
    if (shadow == nullptr) {
        return nullptr;
    }
    const std::uint64_t epoch =
        g_rasterizer_shadow_epoch.load(std::memory_order_acquire);
    if (shadow->epoch != epoch) {
        *shadow = {};
        shadow->context = context;
        shadow->occupied = true;
        shadow->epoch = epoch;
    }
    return shadow;
}

bool rejected_draw_signature_matches(
    const RejectedDrawSignature& left,
    const RejectedDrawSignature& right) {
    return std::memcmp(&left, &right, sizeof(left)) == 0;
}

void record_rejected_draw_locked(
    const RejectedDrawSignature& signature, std::uint64_t draw_serial) {
    for (std::size_t index = 0; index < g_rejected_draw_count; ++index) {
        RejectedDrawAggregate& aggregate = g_rejected_draws[index];
        if (aggregate.occupied &&
            rejected_draw_signature_matches(aggregate.signature, signature)) {
            ++aggregate.hits;
            aggregate.last_frame = g_lifetime_frame_serial;
            aggregate.last_draw = draw_serial;
            return;
        }
    }
    if (g_rejected_draw_count >= g_rejected_draws.size()) {
        ++g_rejected_draw_overflow;
        ++g_draw_proof_diagnostics.rejected_signature_overflow;
        return;
    }
    RejectedDrawAggregate& aggregate =
        g_rejected_draws[g_rejected_draw_count++];
    aggregate = {};
    aggregate.signature = signature;
    aggregate.hits = 1;
    aggregate.first_frame = g_lifetime_frame_serial;
    aggregate.last_frame = g_lifetime_frame_serial;
    aggregate.first_draw = draw_serial;
    aggregate.last_draw = draw_serial;
    aggregate.occupied = true;
}

void reset_final_draw_proof_locked() {
    g_final_draw_proof_signature = {};
    g_final_draw_proof_streak = 0;
    g_final_draw_proof_last_frame = UINT64_MAX;
    g_final_draw_proof_locked = false;
}

void clear_catalog_locked(bool clear_signature) {
    std::memset(g_view_cache.data(), 0, sizeof(g_view_cache));
    std::memset(g_shader_view_cache.data(), 0, sizeof(g_shader_view_cache));
    std::memset(g_resources.data(), 0, sizeof(g_resources));
    g_resource_count = 0;
    g_view_cache_entries = 0;
    g_view_cache_replacements = 0;
    g_resource_overflow = 0;
    g_unsupported_views = 0;
    g_frame_serial = 0;
    g_binding_serial = 0;
    g_draw_proof_draw_serial = 0;
    g_event_count = 0;
    g_uav_event_count = 0;
    g_slot_bindings = 0;
    g_total_resource_bindings = 0;
    g_current_output_identity = nullptr;
    g_current_output_context = nullptr;
    g_current_output_is_backbuffer = false;
    g_pixel_shader_resource_shadow.fill(nullptr);
    g_rasterizer_shadows = {};
    g_rasterizer_shadow_epoch.fetch_add(1u, std::memory_order_acq_rel);
    g_rejected_draws = {};
    g_rejected_draw_count = 0;
    g_rejected_draw_overflow = 0;
    g_pixel_shader_slot_zero_candidate.store(false, std::memory_order_release);
    reset_final_draw_proof_locked();
    g_draw_proof_diagnostics = {};
    g_last_draw_proof_report_at = GetTickCount64();
    InterlockedExchange64(&g_lock_contention_drops, 0);
    g_window_started_at = GetTickCount64();
    if (clear_signature) {
        std::memset(
            g_backbuffer_identities.data(), 0, sizeof(g_backbuffer_identities));
        g_backbuffer_identity_count = 0;
        g_backbuffer_width = 0;
        g_backbuffer_height = 0;
        g_backbuffer_format = 0;
        g_backbuffer_samples = 0;
        g_window_active = false;
        g_lifetime_frame_serial = 0;
        reset_automatic_selection_locked(false);
    }
}

bool resolve_shader_view_locked(
    ID3D11ShaderResourceView* view,
    void** identity,
    UINT* width,
    UINT* height,
    UINT* format) {
    if (view == nullptr || identity == nullptr || width == nullptr ||
        height == nullptr || format == nullptr) {
        return false;
    }
    const std::size_t base = pointer_hash(view, kViewCacheCapacity);
    std::size_t empty_slot = kViewCacheCapacity;
    for (std::size_t probe = 0; probe < kViewCacheMaxProbe; ++probe) {
        const std::size_t slot = (base + probe) & (kViewCacheCapacity - 1);
        const ShaderViewCacheEntry& cached = g_shader_view_cache[slot];
        if (cached.occupied && cached.view == view) {
            *identity = cached.identity;
            *width = cached.width;
            *height = cached.height;
            *format = cached.format;
            return true;
        }
        if (!cached.occupied) {
            empty_slot = slot;
            break;
        }
    }
    ID3D11Resource* resource = nullptr;
    view->GetResource(&resource);
    if (resource == nullptr) {
        return false;
    }
    ID3D11Texture2D* texture = nullptr;
    const HRESULT result = resource->QueryInterface(
        IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture));
    resource->Release();
    if (FAILED(result) || texture == nullptr) {
        return false;
    }
    D3D11_TEXTURE2D_DESC description = {};
    texture->GetDesc(&description);
    D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
    view->GetDesc(&view_description);
    const void* token = normalized_identity(texture);
    texture->Release();
    std::size_t slot = empty_slot == kViewCacheCapacity ? base : empty_slot;
    ShaderViewCacheEntry& entry = g_shader_view_cache[slot];
    entry.view = view;
    entry.identity = const_cast<void*>(token);
    entry.width = description.Width;
    entry.height = description.Height;
    entry.format =
        view_description.Format != DXGI_FORMAT_UNKNOWN
            ? view_description.Format
            : description.Format;
    entry.occupied = true;
    *identity = entry.identity;
    *width = entry.width;
    *height = entry.height;
    *format = entry.format;
    return true;
}

int find_resource_locked(void* identity) {
    for (std::size_t index = 0; index < g_resource_count; ++index) {
        if (g_resources[index].identity == identity) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int resolve_view_locked(ID3D11RenderTargetView* view) {
    if (view == nullptr) {
        return -1;
    }
    const std::size_t base = pointer_hash(view, kViewCacheCapacity);
    std::size_t empty_slot = kViewCacheCapacity;
    for (std::size_t probe = 0; probe < kViewCacheMaxProbe; ++probe) {
        const std::size_t slot = (base + probe) & (kViewCacheCapacity - 1);
        const ViewCacheEntry& cached = g_view_cache[slot];
        if (cached.occupied && cached.view == view) {
            return cached.resource_index;
        }
        if (!cached.occupied) {
            empty_slot = slot;
            break;
        }
    }

    ID3D11Resource* resource = nullptr;
    view->GetResource(&resource);
    if (resource == nullptr) {
        ++g_unsupported_views;
        return -1;
    }
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    resource->GetType(&dimension);
    if (dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        resource->Release();
        ++g_unsupported_views;
        return -1;
    }

    ID3D11Texture2D* texture = nullptr;
    const HRESULT texture_result = resource->QueryInterface(
        IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture));
    if (FAILED(texture_result) || texture == nullptr) {
        resource->Release();
        ++g_unsupported_views;
        return -1;
    }

    D3D11_TEXTURE2D_DESC texture_description = {};
    texture->GetDesc(&texture_description);
    D3D11_RENDER_TARGET_VIEW_DESC view_description = {};
    view->GetDesc(&view_description);
    void* identity = normalized_identity(texture);

    int resource_index = find_resource_locked(identity);
    if (resource_index < 0) {
        if (g_resource_count >= kResourceCapacity) {
            ++g_resource_overflow;
            texture->Release();
            resource->Release();
            return -1;
        }
        resource_index = static_cast<int>(g_resource_count++);
        ColorResourceEntry& entry = g_resources[resource_index];
        entry.identity = identity;
        entry.width = texture_description.Width;
        entry.height = texture_description.Height;
        entry.texture_format = texture_description.Format;
        entry.primary_view_format = view_description.Format;
        entry.sample_count = texture_description.SampleDesc.Count;
        entry.bind_flags = texture_description.BindFlags;
        entry.misc_flags = texture_description.MiscFlags;
        entry.mip_levels = texture_description.MipLevels;
        entry.array_size = texture_description.ArraySize;
        entry.exact_backbuffer_resource =
            is_backbuffer_identity_locked(identity);
    } else if (g_resources[resource_index].primary_view_format !=
               static_cast<UINT>(view_description.Format)) {
        ++g_resources[resource_index].view_format_variants;
    }
    ++g_resources[resource_index].views;

    texture->Release();
    resource->Release();

    std::size_t cache_slot = empty_slot;
    if (cache_slot == kViewCacheCapacity) {
        cache_slot = base;
        if (g_view_cache[cache_slot].occupied) {
            ++g_view_cache_replacements;
        }
    } else {
        ++g_view_cache_entries;
    }
    g_view_cache[cache_slot].view = view;
    g_view_cache[cache_slot].resource_index =
        static_cast<std::uint16_t>(resource_index);
    g_view_cache[cache_slot].occupied = true;
    return resource_index;
}

void register_backbuffer_locked(ID3D11Texture2D* back_buffer) {
    if (back_buffer == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < g_backbuffer_identity_count; ++index) {
        if (g_backbuffer_identities[index].interface_pointer == back_buffer) {
            return;
        }
    }
    if (g_backbuffer_identity_count >= kBackBufferIdentityCapacity) {
        return;
    }
    void* identity = normalized_identity(back_buffer);
    BackBufferIdentityEntry& entry =
        g_backbuffer_identities[g_backbuffer_identity_count++];
    entry.interface_pointer = back_buffer;
    entry.identity = identity;
    for (std::size_t index = 0; index < g_resource_count; ++index) {
        if (g_resources[index].identity == identity) {
            g_resources[index].exact_backbuffer_resource = true;
        }
    }
}

bool take_report_snapshot(ColorReportSnapshot* snapshot) {
    if (snapshot == nullptr) {
        return false;
    }
    if (!TryAcquireSRWLockExclusive(&g_catalog_lock)) {
        InterlockedIncrement64(&g_lock_contention_drops);
        return false;
    }
    if (!g_window_active) {
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return false;
    }
    const ULONGLONG now = GetTickCount64();
    snapshot->resource_count = g_resource_count;
    std::copy_n(
        g_resources.begin(), g_resource_count, snapshot->resources.begin());
    snapshot->backbuffer_width = g_backbuffer_width;
    snapshot->backbuffer_height = g_backbuffer_height;
    snapshot->backbuffer_format = g_backbuffer_format;
    snapshot->backbuffer_samples = g_backbuffer_samples;
    snapshot->frames = g_frame_serial;
    snapshot->serial = g_binding_serial;
    snapshot->event_count = g_event_count;
    snapshot->uav_event_count = g_uav_event_count;
    snapshot->slot_bindings = g_slot_bindings;
    snapshot->total_resource_bindings = g_total_resource_bindings;
    snapshot->view_cache_entries = g_view_cache_entries;
    snapshot->view_cache_replacements = g_view_cache_replacements;
    snapshot->resource_overflow = g_resource_overflow;
    snapshot->unsupported_views = g_unsupported_views;
    snapshot->lock_contention_drops = static_cast<std::uint64_t>(
        InterlockedExchange64(&g_lock_contention_drops, 0));
    snapshot->elapsed_milliseconds = now - g_window_started_at;
    clear_catalog_locked(false);
    ReleaseSRWLockExclusive(&g_catalog_lock);
    return true;
}

const char* report_reason_name(ColorReportReason reason) {
    switch (reason) {
        case ColorReportReason::window_complete:
            return "window-complete";
        case ColorReportReason::device_shutdown:
            return "device-shutdown";
        case ColorReportReason::device_replaced:
            return "device-replaced";
        default:
            return "unknown";
    }
}

void write_color_report(
    ColorReportSnapshot* snapshot, ColorReportReason reason) {
    if (snapshot == nullptr) {
        return;
    }
    std::sort(
        snapshot->resources.begin(),
        snapshot->resources.begin() + snapshot->resource_count,
        [](const ColorResourceEntry& left, const ColorResourceEntry& right) {
            if (left.bindings != right.bindings) {
                return left.bindings > right.bindings;
            }
            const std::uint64_t left_area =
                static_cast<std::uint64_t>(left.width) * left.height;
            const std::uint64_t right_area =
                static_cast<std::uint64_t>(right.width) * right.height;
            return left_area > right_area;
        });

    log_message(
        "Relatorio color 0.7.0: reason=%s janela=%llums frames=%llu "
        "events=%llu uav_events=%llu slot_bindings=%llu resources=%llu "
        "views=%llu cache_replacements=%llu resource_overflow=%llu "
        "unsupported_views=%llu contention_drops=%llu async_job_drops=%llu "
        "report_queue_drops=%llu backbuffer=%ux%u format=%u(%s) samples=%u.",
        report_reason_name(reason),
        static_cast<unsigned long long>(snapshot->elapsed_milliseconds),
        static_cast<unsigned long long>(snapshot->frames),
        static_cast<unsigned long long>(snapshot->event_count),
        static_cast<unsigned long long>(snapshot->uav_event_count),
        static_cast<unsigned long long>(snapshot->slot_bindings),
        static_cast<unsigned long long>(snapshot->resource_count),
        static_cast<unsigned long long>(snapshot->view_cache_entries),
        static_cast<unsigned long long>(snapshot->view_cache_replacements),
        static_cast<unsigned long long>(snapshot->resource_overflow),
        static_cast<unsigned long long>(snapshot->unsupported_views),
        static_cast<unsigned long long>(snapshot->lock_contention_drops),
        static_cast<unsigned long long>(snapshot->async_job_drops),
        static_cast<unsigned long long>(snapshot->report_queue_drops),
        snapshot->backbuffer_width,
        snapshot->backbuffer_height,
        snapshot->backbuffer_format,
        format_name(snapshot->backbuffer_format),
        snapshot->backbuffer_samples);

    const std::size_t reported = std::min(
        snapshot->resource_count, kMaximumReportedTargets);
    for (std::size_t index = 0; index < reported; ++index) {
        const ColorResourceEntry& entry = snapshot->resources[index];
        const auto evidence = photorealism::fsr::classify_color_target(
            photorealism::fsr::ColorEvidenceInput{
                entry.width,
                entry.height,
                snapshot->backbuffer_width,
                snapshot->backbuffer_height,
                entry.views,
                entry.bindings,
                entry.slot_zero_bindings,
                snapshot->total_resource_bindings,
                entry.first_serial,
                entry.last_serial,
                snapshot->serial,
                entry.exact_backbuffer_resource});
        const double seconds = snapshot->elapsed_milliseconds > 0
                                   ? snapshot->elapsed_milliseconds / 1000.0
                                   : 1.0;
        log_message(
            "Color target #%llu: evidence=%s confidence=%u "
            "scores(scene=%u reflection=%u interface=%u) resource=%p "
            "size=%ux%u area=%u.%u%% aspect_error=%u.%u%% "
            "texture_format=%u(%s) view_format=%u(%s) "
            "view_format_variants=%u samples=%u bind_flags=0x%08X "
            "misc_flags=0x%08X mips=%u array=%u views=%u bindings=%llu "
            "rate=%.1f/s slot0=%llu slot_mask=0x%02X "
            "order=%llu-%llu frames=%llu-%llu event_flags=0x%X "
            "exact_backbuffer=%s direct_composition_hits=%llu.",
            static_cast<unsigned long long>(index + 1),
            photorealism::fsr::color_evidence_label_name(evidence.label),
            evidence.confidence,
            evidence.scene_score,
            evidence.reflection_score,
            evidence.interface_score,
            entry.identity,
            entry.width,
            entry.height,
            evidence.area_per_mille / 10,
            evidence.area_per_mille % 10,
            evidence.aspect_error_per_mille / 10,
            evidence.aspect_error_per_mille % 10,
            entry.texture_format,
            format_name(entry.texture_format),
            entry.primary_view_format,
            format_name(entry.primary_view_format),
            entry.view_format_variants,
            entry.sample_count,
            entry.bind_flags,
            entry.misc_flags,
            entry.mip_levels,
            entry.array_size,
            entry.views,
            static_cast<unsigned long long>(entry.bindings),
            entry.bindings / seconds,
            static_cast<unsigned long long>(entry.slot_zero_bindings),
            entry.slot_mask,
            static_cast<unsigned long long>(entry.first_serial),
            static_cast<unsigned long long>(entry.last_serial),
            static_cast<unsigned long long>(entry.first_frame),
            static_cast<unsigned long long>(entry.last_frame),
            entry.event_flags,
            entry.exact_backbuffer_resource ? "sim" : "nao",
            static_cast<unsigned long long>(entry.direct_composition_hits));
    }
    if (reported < snapshot->resource_count) {
        log_message(
            "Relatorio color 0.7.0 limitado aos %llu alvos mais ativos; "
            "%llu recursos de baixa atividade omitidos.",
            static_cast<unsigned long long>(reported),
            static_cast<unsigned long long>(
                snapshot->resource_count - reported));
    }
    log_message(
        "Rotulos color 0.7.0 sao hipoteses por evidencia observavel; "
        "nenhum bind e prova de draw. Substituicao permanece bloqueada ate "
        "existir validacao de viewport, scissor e shader no draw final.");
}

int reserve_diagnostic_job(DiagnosticJobType type) {
    AcquireSRWLockExclusive(&g_diagnostic_queue_lock);
    if (!g_diagnostic_accepting || g_diagnostic_thread == nullptr) {
        ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
        InterlockedIncrement64(&g_async_job_drops);
        return -1;
    }
    for (std::size_t index = 0; index < g_diagnostic_jobs.size(); ++index) {
        DiagnosticJob& job = g_diagnostic_jobs[index];
        if (job.state == DiagnosticJobState::empty) {
            job.state = DiagnosticJobState::filling;
            job.type = type;
            ++g_diagnostic_active_fillers;
            ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
            return static_cast<int>(index);
        }
    }
    ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
    InterlockedIncrement64(&g_async_job_drops);
    return -1;
}

void cancel_diagnostic_job(int index) {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= g_diagnostic_jobs.size()) {
        return;
    }
    AcquireSRWLockExclusive(&g_diagnostic_queue_lock);
    DiagnosticJob& job = g_diagnostic_jobs[static_cast<std::size_t>(index)];
    job.state = DiagnosticJobState::empty;
    job.type = DiagnosticJobType::none;
    if (g_diagnostic_active_fillers > 0) {
        --g_diagnostic_active_fillers;
    }
    WakeAllConditionVariable(&g_diagnostic_fill_condition);
    ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
}

bool publish_diagnostic_job(int index) {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= g_diagnostic_jobs.size()) {
        return false;
    }
    AcquireSRWLockExclusive(&g_diagnostic_queue_lock);
    DiagnosticJob& job = g_diagnostic_jobs[static_cast<std::size_t>(index)];
    if (!g_diagnostic_accepting ||
        job.state != DiagnosticJobState::filling ||
        g_diagnostic_event == nullptr) {
        job.state = DiagnosticJobState::empty;
        job.type = DiagnosticJobType::none;
        if (g_diagnostic_active_fillers > 0) {
            --g_diagnostic_active_fillers;
        }
        WakeAllConditionVariable(&g_diagnostic_fill_condition);
        ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
        InterlockedIncrement64(&g_async_job_drops);
        return false;
    }
    job.state = DiagnosticJobState::pending;
    if (g_diagnostic_active_fillers > 0) {
        --g_diagnostic_active_fillers;
    }
    WakeAllConditionVariable(&g_diagnostic_fill_condition);
    SetEvent(g_diagnostic_event);
    ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
    return true;
}

void discard_current_report_window() {
    AcquireSRWLockExclusive(&g_catalog_lock);
    if (g_window_active) {
        clear_catalog_locked(false);
    }
    ReleaseSRWLockExclusive(&g_catalog_lock);
}

bool enqueue_color_report(ColorReportReason reason) {
    const int index = reserve_diagnostic_job(DiagnosticJobType::color_report);
    if (index < 0) {
        InterlockedIncrement64(&g_report_queue_drops);
        discard_current_report_window();
        return false;
    }
    DiagnosticJob& job = g_diagnostic_jobs[static_cast<std::size_t>(index)];
    job.report_reason = reason;
    if (!take_report_snapshot(&job.snapshot)) {
        cancel_diagnostic_job(index);
        return false;
    }
    job.snapshot.async_job_drops = static_cast<std::uint64_t>(
        InterlockedExchange64(&g_async_job_drops, 0));
    job.snapshot.report_queue_drops = static_cast<std::uint64_t>(
        InterlockedExchange64(&g_report_queue_drops, 0));
    return publish_diagnostic_job(index);
}

void enqueue_window_notice(
    bool signature_changed,
    UINT width,
    UINT height,
    UINT format,
    UINT sample_count) {
    const int index = reserve_diagnostic_job(DiagnosticJobType::window_notice);
    if (index < 0) {
        return;
    }
    DiagnosticJob& job = g_diagnostic_jobs[static_cast<std::size_t>(index)];
    job.signature_changed = signature_changed;
    job.width = width;
    job.height = height;
    job.format = format;
    job.sample_count = sample_count;
    publish_diagnostic_job(index);
}

void enqueue_reset_notice(std::uint32_t reason) {
    const int index = reserve_diagnostic_job(DiagnosticJobType::reset_notice);
    if (index < 0) {
        return;
    }
    DiagnosticJob& job = g_diagnostic_jobs[static_cast<std::size_t>(index)];
    job.reset_reason = reason;
    publish_diagnostic_job(index);
}

void enqueue_automatic_selection_notice(
    AutomaticSelectionNotice notice,
    const AutomaticSelectionState& selection) {
    const DiagnosticJobType type =
        notice == AutomaticSelectionNotice::ready
            ? DiagnosticJobType::automatic_selection_ready
            : DiagnosticJobType::automatic_selection_fallback;
    const int index = reserve_diagnostic_job(type);
    if (index < 0) {
        return;
    }
    DiagnosticJob& job = g_diagnostic_jobs[static_cast<std::size_t>(index)];
    job.generation = selection.generation;
    job.source_width = selection.source_width;
    job.source_height = selection.source_height;
    job.format = selection.source_format;
    job.width = selection.output_width;
    job.height = selection.output_height;
    job.confidence = selection.confidence;
    publish_diagnostic_job(index);
}

void enqueue_fsr_active_notice(const AutomaticSelectionState& selection) {
    const int index = reserve_diagnostic_job(DiagnosticJobType::fsr_active);
    if (index < 0) {
        return;
    }
    DiagnosticJob& job = g_diagnostic_jobs[static_cast<std::size_t>(index)];
    job.generation = selection.generation;
    job.source_width = selection.source_width;
    job.source_height = selection.source_height;
    job.format = selection.source_format;
    job.width = selection.output_width;
    job.height = selection.output_height;
    job.confidence = selection.confidence;
    job.fsr_upscale_enabled =
        photorealism::fsr::evaluate_fsr_execution_policy(
            selection.source_width,
            selection.source_height,
            selection.output_width,
            selection.output_height)
            .execute;
    publish_diagnostic_job(index);
}

void enqueue_fsr_timing(const FsrGpuTimingSummary& summary) {
    if (summary.samples == 0 && summary.dropped == 0) {
        return;
    }
    const int index = reserve_diagnostic_job(DiagnosticJobType::fsr_timing);
    if (index < 0) {
        return;
    }
    DiagnosticJob& job = g_diagnostic_jobs[static_cast<std::size_t>(index)];
    job.timing_samples = summary.samples;
    job.timing_dropped = summary.dropped;
    job.easu_sum_ms = summary.easu_sum_ms;
    job.rcas_sum_ms = summary.rcas_sum_ms;
    job.temporal_aa_sum_ms = summary.temporal_aa_sum_ms;
    job.easu_max_ms = summary.easu_max_ms;
    job.rcas_max_ms = summary.rcas_max_ms;
    job.temporal_aa_max_ms = summary.temporal_aa_max_ms;
    publish_diagnostic_job(index);
}

void enqueue_draw_proof_report(const DrawProofReportSnapshot& snapshot) {
    const int index = reserve_diagnostic_job(DiagnosticJobType::draw_proof_report);
    if (index < 0) {
        return;
    }
    g_diagnostic_jobs[static_cast<std::size_t>(index)].draw_proof_report =
        snapshot;
    publish_diagnostic_job(index);
}

bool process_one_diagnostic_job() {
    std::size_t selected = g_diagnostic_jobs.size();
    AcquireSRWLockExclusive(&g_diagnostic_queue_lock);
    for (std::size_t index = 0; index < g_diagnostic_jobs.size(); ++index) {
        if (g_diagnostic_jobs[index].state == DiagnosticJobState::pending) {
            selected = index;
            g_diagnostic_jobs[index].state = DiagnosticJobState::processing;
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
    if (selected == g_diagnostic_jobs.size()) {
        return false;
    }

    DiagnosticJob& job = g_diagnostic_jobs[selected];
    switch (job.type) {
        case DiagnosticJobType::color_report:
            write_color_report(&job.snapshot, job.report_reason);
            break;
        case DiagnosticJobType::window_notice:
            log_message(
                "Janela color 0.7.0 %s: backbuffer=%ux%u format=%u(%s) "
                "samples=%u; captura exclusivamente diagnostica.",
                job.signature_changed ? "reiniciada por assinatura" : "iniciada",
                job.width,
                job.height,
                job.format,
                format_name(job.format),
                job.sample_count);
            break;
        case DiagnosticJobType::reset_notice:
            log_message(
                "Janela color 0.7.0 limpa: reason=%s; recursos nao foram "
                "retidos.",
                job.reset_reason == PHOTOREALISM_FSR_RESET_RESIZE
                    ? "resize"
                    : (job.reset_reason ==
                               PHOTOREALISM_FSR_RESET_PLUGIN_DISABLED
                           ? "Home-disabled"
                           : "unknown"));
            break;
        case DiagnosticJobType::automatic_selection_ready:
            log_message(
                "AA/FSR source selecionado: source=%ux%u format=%u(%s) "
                "output=%ux%u confidence=%u generation=%llu; selecao "
                "automatica estabilizada por composicao direta; execucao "
                "ainda depende dos gates runtime.",
                job.source_width,
                job.source_height,
                job.format,
                format_name(job.format),
                job.width,
                job.height,
                job.confidence,
                static_cast<unsigned long long>(job.generation));
            break;
        case DiagnosticJobType::automatic_selection_fallback:
            log_message(
                "AA/FSR automatico voltou ao fallback pass-through: ultimo "
                "source=%ux%u format=%u(%s) output=%ux%u generation=%llu; "
                "observacao continua sem alterar frames.",
                job.source_width,
                job.source_height,
                job.format,
                format_name(job.format),
                job.width,
                job.height,
                static_cast<unsigned long long>(job.generation));
            break;
        case DiagnosticJobType::fsr_active:
            log_message(
                "AA Photorealism ativo antes da UI: spatial=edge-aware "
                "temporal=history-clamp+screenspace-3x3 "
                "engine_motion_vectors=indisponiveis jitter=indisponivel; "
                "source=%ux%u format=%u(%s) output=%ux%u confidence=%u "
                "generation=%llu EASU=%s RCAS=ativo(0.4-stops).",
                job.source_width,
                job.source_height,
                job.format,
                format_name(job.format),
                job.width,
                job.height,
                job.confidence,
                static_cast<unsigned long long>(job.generation),
                job.fsr_upscale_enabled ? "ativo" : "pass-through-nativo");
            break;
        case DiagnosticJobType::fsr_timing:
            log_message(
                "Telemetria GPU AA/FSR 0.7.0: samples=%llu dropped=%llu "
                "TemporalAA_avg=%.3fms TemporalAA_max=%.3fms "
                "EASU_avg=%.3fms EASU_max=%.3fms RCAS_avg=%.3fms "
                "RCAS_max=%.3fms.",
                static_cast<unsigned long long>(job.timing_samples),
                static_cast<unsigned long long>(job.timing_dropped),
                job.timing_samples != 0
                    ? job.temporal_aa_sum_ms / job.timing_samples
                    : 0.0,
                job.temporal_aa_max_ms,
                job.timing_samples != 0
                    ? job.easu_sum_ms / job.timing_samples
                    : 0.0,
                job.easu_max_ms,
                job.timing_samples != 0
                    ? job.rcas_sum_ms / job.timing_samples
                    : 0.0,
                job.rcas_max_ms);
            break;
        case DiagnosticJobType::draw_proof_report:
            write_draw_proof_report(job.draw_proof_report);
            break;
        default:
            break;
    }

    AcquireSRWLockExclusive(&g_diagnostic_queue_lock);
    job.state = DiagnosticJobState::empty;
    job.type = DiagnosticJobType::none;
    ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
    return true;
}

DWORD WINAPI diagnostic_worker(LPVOID) {
    for (;;) {
        HANDLE events[2] = {g_diagnostic_stop_event, g_diagnostic_event};
        const DWORD wait_result =
            WaitForMultipleObjects(2, events, FALSE, INFINITE);
        const bool stopping = wait_result == WAIT_OBJECT_0;
        if (wait_result != WAIT_OBJECT_0 &&
            wait_result != WAIT_OBJECT_0 + 1) {
            break;
        }
        while (process_one_diagnostic_job()) {
        }
        if (stopping) {
            break;
        }
    }
    return 0;
}

bool start_diagnostic_worker() {
    AcquireSRWLockExclusive(&g_diagnostic_queue_lock);
    if (g_diagnostic_thread != nullptr) {
        const bool running = g_diagnostic_accepting;
        ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
        return running;
    }
    for (DiagnosticJob& job : g_diagnostic_jobs) {
        job.state = DiagnosticJobState::empty;
        job.type = DiagnosticJobType::none;
    }
    g_diagnostic_active_fillers = 0;
    g_diagnostic_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_diagnostic_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_diagnostic_event == nullptr || g_diagnostic_stop_event == nullptr) {
        if (g_diagnostic_event != nullptr) {
            CloseHandle(g_diagnostic_event);
        }
        if (g_diagnostic_stop_event != nullptr) {
            CloseHandle(g_diagnostic_stop_event);
        }
        g_diagnostic_event = nullptr;
        g_diagnostic_stop_event = nullptr;
        ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
        return false;
    }
    g_diagnostic_thread =
        CreateThread(nullptr, 0, diagnostic_worker, nullptr, 0, nullptr);
    if (g_diagnostic_thread == nullptr) {
        CloseHandle(g_diagnostic_event);
        CloseHandle(g_diagnostic_stop_event);
        g_diagnostic_event = nullptr;
        g_diagnostic_stop_event = nullptr;
        ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
        return false;
    }
    g_diagnostic_accepting = true;
    InterlockedExchange64(&g_async_job_drops, 0);
    InterlockedExchange64(&g_report_queue_drops, 0);
    ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
    return true;
}

void stop_diagnostic_worker() {
    AcquireSRWLockExclusive(&g_diagnostic_queue_lock);
    HANDLE thread = g_diagnostic_thread;
    HANDLE stop_event = g_diagnostic_stop_event;
    HANDLE wake_event = g_diagnostic_event;
    g_diagnostic_accepting = false;
    while (g_diagnostic_active_fillers != 0) {
        SleepConditionVariableSRW(
            &g_diagnostic_fill_condition,
            &g_diagnostic_queue_lock,
            INFINITE,
            0);
    }
    if (stop_event != nullptr) {
        SetEvent(stop_event);
    }
    if (wake_event != nullptr) {
        SetEvent(wake_event);
    }
    ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);

    if (thread != nullptr) {
        WaitForSingleObject(thread, INFINITE);
    }

    AcquireSRWLockExclusive(&g_diagnostic_queue_lock);
    if (thread != nullptr) {
        CloseHandle(thread);
    }
    if (stop_event != nullptr) {
        CloseHandle(stop_event);
    }
    if (wake_event != nullptr) {
        CloseHandle(wake_event);
    }
    g_diagnostic_thread = nullptr;
    g_diagnostic_stop_event = nullptr;
    g_diagnostic_event = nullptr;
    for (DiagnosticJob& job : g_diagnostic_jobs) {
        job.state = DiagnosticJobState::empty;
        job.type = DiagnosticJobType::none;
    }
    ReleaseSRWLockExclusive(&g_diagnostic_queue_lock);
}

HRESULT WINAPI initialize_device(ID3D11Device* device) {
    if (device == nullptr) {
        return E_INVALIDARG;
    }

    const bool diagnostic_worker_ready = start_diagnostic_worker();

    AcquireSRWLockExclusive(&g_catalog_lock);
    if (g_device == device) {
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return S_FALSE;
    }
    const bool replacing_device = g_device != nullptr;
    ReleaseSRWLockExclusive(&g_catalog_lock);

    if (replacing_device) {
        enqueue_color_report(ColorReportReason::device_replaced);
    }

    ID3D11DeviceContext* immediate_context = nullptr;
    device->GetImmediateContext(&immediate_context);
    AcquireSRWLockExclusive(&g_catalog_lock);
    ID3D11Device* previous = g_device;
    ID3D11DeviceContext* previous_immediate_context = g_immediate_context;
    device->AddRef();
    g_device = device;
    g_immediate_context = immediate_context;
    clear_catalog_locked(true);
    ReleaseSRWLockExclusive(&g_catalog_lock);
    if (previous != nullptr) {
        previous->Release();
        log_message("Dispositivo anterior liberado antes da reinicializacao.");
    }
    if (previous_immediate_context != nullptr) {
        previous_immediate_context->Release();
    }

    AcquireSRWLockExclusive(&g_runtime_lock);
    shutdown_fsr_runtime();
    const bool fsr_runtime_ready = initialize_fsr_runtime(device, g_module);
    ReleaseSRWLockExclusive(&g_runtime_lock);
    AcquireSRWLockExclusive(&g_catalog_lock);
    g_fsr_runtime_available = fsr_runtime_ready;
    g_last_timing_report_at = GetTickCount64();
    ReleaseSRWLockExclusive(&g_catalog_lock);

    const D3D_FEATURE_LEVEL feature_level = device->GetFeatureLevel();
    log_message(
        "Photorealism FSR/AA 0.7.1 inicializado: ABI=v1+v2+v3+v4+v5+v6 "
        "feature_level=%s(0x%X) device=%p.",
        feature_level_name(feature_level),
        static_cast<unsigned>(feature_level),
        static_cast<void*>(device));
    log_adapter(device);

    D3D11_FEATURE_DATA_THREADING threading = {};
    HRESULT result = device->CheckFeatureSupport(
        D3D11_FEATURE_THREADING, &threading, sizeof(threading));
    if (SUCCEEDED(result)) {
        log_message(
            "Capacidades D3D11: concurrent_creates=%s command_lists=%s.",
            threading.DriverConcurrentCreates ? "sim" : "nao",
            threading.DriverCommandLists ? "sim" : "nao");
    } else {
        log_message(
            "Capacidades de threading indisponiveis: result=0x%08X.",
            static_cast<unsigned>(result));
    }

    D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS compute_options = {};
    result = device->CheckFeatureSupport(
        D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS,
        &compute_options,
        sizeof(compute_options));
    log_message(
        "Compute shader 4.x raw/structured=%s (query=0x%08X); "
        "feature level 11.x ou superior habilita compute SM5 planejado.",
        SUCCEEDED(result) &&
                compute_options
                    .ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x
            ? "sim"
            : "nao",
        static_cast<unsigned>(result));

    log_format_support(
        device, DXGI_FORMAT_R16G16B16A16_FLOAT, "R16G16B16A16_FLOAT");
    log_format_support(
        device, DXGI_FORMAT_R11G11B10_FLOAT, "R11G11B10_FLOAT");
    log_format_support(
        device, DXGI_FORMAT_R8G8B8A8_UNORM, "R8G8B8A8_UNORM");
    log_message(
        "Observador color 0.7.1 pronto: janela=30000ms resources=256 "
        "view_cache=4096 max_report=32 diagnostic_queue=2 worker=%s; "
        "consultas COM apenas em cache miss.",
        diagnostic_worker_ready ? "ativo" : "indisponivel");
    log_message(
        "AA/FSR 0.7.1: PS binds sao somente pistas; o draw final exige "
        "RTV/backbuffer, viewport, scissor, shader e topologia validos. "
        "Esta entrega e diagnostica: nenhuma substituicao ou dispatch ocorre.");
    return S_OK;
}

void WINAPI observe_color_targets(
    const PhotorealismFsrColorTargetsEventV2* event) {
    if (event == nullptr || event->struct_size < sizeof(*event) ||
        (event->render_target_count != 0 && event->render_targets == nullptr)) {
        return;
    }
    if (!TryAcquireSRWLockExclusive(&g_catalog_lock)) {
        InterlockedIncrement64(&g_lock_contention_drops);
        return;
    }
    if (g_device == nullptr || !g_window_active) {
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return;
    }

    g_current_output_identity = nullptr;
    g_current_output_context =
        event->struct_size >= sizeof(PhotorealismFsrColorTargetsEventV4)
            ? reinterpret_cast<const PhotorealismFsrColorTargetsEventV4*>(event)
                  ->context
            : nullptr;
    g_current_output_is_backbuffer = false;
    if (event->render_target_count == 0) {
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return;
    }

    ++g_binding_serial;
    ++g_event_count;
    if ((event->flags &
         PHOTOREALISM_FSR_COLOR_EVENT_OM_SET_RENDER_TARGETS_AND_UAVS) != 0) {
        ++g_uav_event_count;
    }
    const UINT count = std::min<UINT>(
        event->render_target_count, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
    std::array<int, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> unique_resources =
        {};
    unique_resources.fill(-1);
    UINT unique_count = 0;
    for (UINT slot = 0; slot < count; ++slot) {
        ID3D11RenderTargetView* view = event->render_targets[slot];
        if (view == nullptr) {
            continue;
        }
        ++g_slot_bindings;
        const int resource_index = resolve_view_locked(view);
        if (resource_index < 0) {
            continue;
        }
        bool already_seen = false;
        for (UINT seen = 0; seen < unique_count; ++seen) {
            if (unique_resources[seen] == resource_index) {
                already_seen = true;
                break;
            }
        }
        ColorResourceEntry& resource = g_resources[resource_index];
        if (slot == 0) {
            g_current_output_identity = resource.identity;
            g_current_output_is_backbuffer =
                resource.exact_backbuffer_resource;
        }
        resource.slot_mask |= 1u << slot;
        resource.event_flags |= event->flags;
        if (slot == 0) {
            ++resource.slot_zero_bindings;
        }
        if (already_seen) {
            continue;
        }
        unique_resources[unique_count++] = resource_index;
        ++resource.bindings;
        ++g_total_resource_bindings;
        if (resource.first_serial == 0) {
            resource.first_serial = g_binding_serial;
            resource.first_frame = g_frame_serial;
        }
        resource.last_serial = g_binding_serial;
        resource.last_frame = g_frame_serial;
    }
    ReleaseSRWLockExclusive(&g_catalog_lock);
}

void WINAPI observe_frame(const PhotorealismFsrFrameEventV2* event) {
    if (event == nullptr || event->struct_size < sizeof(*event) ||
        event->back_buffer == nullptr || event->width == 0 ||
        event->height == 0) {
        return;
    }
    bool started = false;
    bool signature_changed = false;
    bool report_due = false;
    AutomaticSelectionNotice selection_notice =
        AutomaticSelectionNotice::none;
    AutomaticSelectionState selection_snapshot = {};
    AutomaticSelectionState current_selection = {};
    bool fsr_active_notice = false;
    AutomaticSelectionState fsr_active_snapshot = {};
    bool draw_proof_report_due = false;
    DrawProofReportSnapshot draw_proof_report = {};
    AcquireSRWLockExclusive(&g_catalog_lock);
    if (g_device == nullptr) {
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return;
    }
    if (!g_window_active || g_backbuffer_width != event->width ||
        g_backbuffer_height != event->height ||
        g_backbuffer_format != event->format ||
        g_backbuffer_samples != event->sample_count) {
        signature_changed = g_window_active;
        clear_catalog_locked(true);
        g_backbuffer_width = event->width;
        g_backbuffer_height = event->height;
        g_backbuffer_format = event->format;
        g_backbuffer_samples = event->sample_count;
        g_window_active = true;
        g_window_started_at = GetTickCount64();
        started = true;
    }
    register_backbuffer_locked(event->back_buffer);
    ++g_frame_serial;
    ++g_lifetime_frame_serial;
    if (g_final_draw_proof_streak != 0u &&
        g_final_draw_proof_last_frame != UINT64_MAX &&
        g_lifetime_frame_serial >
            g_final_draw_proof_last_frame + kFinalDrawProofLostGraceFrames) {
        reset_final_draw_proof_locked();
    }
    if (g_automatic_selection_notice != AutomaticSelectionNotice::none) {
        selection_notice = g_automatic_selection_notice;
        selection_snapshot = g_automatic_selection_notice_state;
        g_automatic_selection_notice = AutomaticSelectionNotice::none;
        g_automatic_selection_notice_state = {};
    }
    if (g_fsr_execution_notice_pending) {
        fsr_active_notice = true;
        fsr_active_snapshot = g_automatic_selection;
        g_fsr_execution_notice_pending = false;
    }
    current_selection = g_automatic_selection;
    report_due =
        GetTickCount64() - g_window_started_at >=
        kObservationWindowMilliseconds;
    const ULONGLONG now = GetTickCount64();
    if (now - g_last_draw_proof_report_at >= kDrawProofReportMilliseconds) {
        draw_proof_report_due = true;
        draw_proof_report.diagnostics = g_draw_proof_diagnostics;
        draw_proof_report.streak = g_final_draw_proof_streak;
        draw_proof_report.locked = g_final_draw_proof_locked;
        draw_proof_report.source_identity =
            g_final_draw_proof_signature.source_identity;
        draw_proof_report.source_width =
            g_final_draw_proof_signature.source_width;
        draw_proof_report.source_height =
            g_final_draw_proof_signature.source_height;
        draw_proof_report.output_width =
            g_final_draw_proof_signature.output_width;
        draw_proof_report.output_height =
            g_final_draw_proof_signature.output_height;
        draw_proof_report.rejected_draws = g_rejected_draws;
        draw_proof_report.rejected_draw_count = g_rejected_draw_count;
        draw_proof_report.rejected_draw_overflow = g_rejected_draw_overflow;
        g_draw_proof_diagnostics = {};
        g_rejected_draws = {};
        g_rejected_draw_count = 0;
        g_rejected_draw_overflow = 0;
        g_last_draw_proof_report_at = now;
    }
    ReleaseSRWLockExclusive(&g_catalog_lock);

    if (started) {
        enqueue_window_notice(
            signature_changed,
            event->width,
            event->height,
            event->format,
            event->sample_count);
        if (TryAcquireSRWLockExclusive(&g_runtime_lock)) {
            discard_fsr_runtime_output();
            ReleaseSRWLockExclusive(&g_runtime_lock);
        }
    }
    if (report_due) {
        enqueue_color_report(ColorReportReason::window_complete);
    }
    if (draw_proof_report_due) {
        enqueue_draw_proof_report(draw_proof_report);
    }
    if (selection_notice != AutomaticSelectionNotice::none) {
        enqueue_automatic_selection_notice(
            selection_notice, selection_snapshot);
    }
    if (selection_notice == AutomaticSelectionNotice::fallback &&
        TryAcquireSRWLockExclusive(&g_runtime_lock)) {
        discard_fsr_runtime_output();
        ReleaseSRWLockExclusive(&g_runtime_lock);
    }
    if (kEnableFsrRuntimeAfterDrawProof && current_selection.ready) {
        const auto policy = photorealism::fsr::evaluate_fsr_execution_policy(
            current_selection.source_width,
            current_selection.source_height,
            current_selection.output_width,
            current_selection.output_height);
        if (TryAcquireSRWLockExclusive(&g_runtime_lock)) {
            prepare_fsr_runtime_output(
                current_selection.source_width,
                current_selection.source_height,
                current_selection.output_width,
                current_selection.output_height,
                policy.execute);
            ReleaseSRWLockExclusive(&g_runtime_lock);
        }
    }
    if (fsr_active_notice) {
        enqueue_fsr_active_notice(fsr_active_snapshot);
    }
    if (TryAcquireSRWLockExclusive(&g_runtime_lock)) {
        poll_fsr_gpu_timing();
        if (now - g_last_timing_report_at >= 10000u) {
            const FsrGpuTimingSummary timing =
                take_fsr_gpu_timing_summary();
            g_last_timing_report_at = now;
            ReleaseSRWLockExclusive(&g_runtime_lock);
            enqueue_fsr_timing(timing);
        } else {
            ReleaseSRWLockExclusive(&g_runtime_lock);
        }
    }
}

HRESULT WINAPI update_automatic_selection(
    const PhotorealismFsrSelectionContextV3* context,
    PhotorealismFsrAutomaticSelectionV3* selection) {
    if (context == nullptr || context->struct_size < sizeof(*context) ||
        selection == nullptr || selection->struct_size < sizeof(*selection) ||
        context->backbuffer_width == 0 || context->backbuffer_height == 0) {
        return E_INVALIDARG;
    }
    std::memset(selection, 0, sizeof(*selection));
    selection->struct_size = sizeof(*selection);

    if (!TryAcquireSRWLockExclusive(&g_catalog_lock)) {
        InterlockedIncrement64(&g_lock_contention_drops);
        return S_FALSE;
    }
    if (g_device == nullptr || !g_window_active) {
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return S_FALSE;
    }
    int best_index = -1;
    photorealism::fsr::AutomaticSceneCandidateResult best_score = {};
    for (std::size_t index = 0; index < g_resource_count; ++index) {
        const ColorResourceEntry& entry = g_resources[index];
        const UINT candidate_format =
            photorealism::fsr::is_fsr_color_format(entry.texture_format)
                ? entry.texture_format
                : entry.primary_view_format;
        const auto score =
            photorealism::fsr::score_automatic_scene_candidate({
                entry.width,
                entry.height,
                candidate_format,
                entry.sample_count,
                entry.bind_flags,
                entry.misc_flags,
                entry.mip_levels,
                entry.array_size,
                context->backbuffer_width,
                context->backbuffer_height,
                context->depth_width,
                context->depth_height,
                entry.bindings,
                entry.slot_zero_bindings,
                entry.last_serial,
                g_binding_serial,
                entry.last_frame,
                g_frame_serial,
                entry.direct_composition_hits,
                entry.last_composition_frame,
                entry.exact_backbuffer_resource});
        if (!score.eligible) {
            continue;
        }
        if (best_index < 0 ||
            photorealism::fsr::automatic_candidate_is_better(
                score,
                entry.bindings,
                entry.last_serial,
                best_score,
                g_resources[static_cast<std::size_t>(best_index)].bindings,
                g_resources[static_cast<std::size_t>(best_index)].last_serial)) {
            best_index = static_cast<int>(index);
            best_score = score;
        }
    }

    if (best_index >= 0) {
        const ColorResourceEntry& best =
            g_resources[static_cast<std::size_t>(best_index)];
        const bool same_candidate =
            g_automatic_selection.identity == best.identity &&
            photorealism::fsr::automatic_candidate_signature_matches(
                {
                    g_automatic_selection.source_width,
                    g_automatic_selection.source_height,
                    g_automatic_selection.source_format,
                    g_automatic_selection.output_width,
                    g_automatic_selection.output_height,
                },
                {
                    best.width,
                    best.height,
                    photorealism::fsr::is_fsr_color_format(best.texture_format)
                        ? best.texture_format
                        : best.primary_view_format,
                    context->backbuffer_width,
                    context->backbuffer_height,
                });
        if (same_candidate) {
            if (g_automatic_selection.confirmations < UINT32_MAX) {
                ++g_automatic_selection.confirmations;
            }
        } else {
            if (g_automatic_selection.ready) {
                g_automatic_selection_notice =
                    AutomaticSelectionNotice::fallback;
                g_automatic_selection_notice_state = g_automatic_selection;
            }
            const std::uint64_t next_generation =
                g_automatic_selection.generation + 1u;
            g_automatic_selection = {};
            g_automatic_selection.identity = best.identity;
            g_automatic_selection.generation = next_generation;
            g_automatic_selection.confirmations = 1;
        }
        g_automatic_selection.source_width = best.width;
        g_automatic_selection.source_height = best.height;
        g_automatic_selection.source_format =
            photorealism::fsr::is_fsr_color_format(best.texture_format)
                ? best.texture_format
                : best.primary_view_format;
        g_automatic_selection.output_width = context->backbuffer_width;
        g_automatic_selection.output_height = context->backbuffer_height;
        g_automatic_selection.confidence = best_score.confidence;
        g_automatic_selection.last_seen_frame = g_lifetime_frame_serial;

        if (!g_automatic_selection.ready &&
            photorealism::fsr::automatic_candidate_can_lock(best_score) &&
            g_automatic_selection.confirmations >=
                kAutomaticSelectionConfirmFrames) {
            g_automatic_selection.ready = true;
            g_automatic_selection_notice = AutomaticSelectionNotice::ready;
            g_automatic_selection_notice_state = g_automatic_selection;
        }
    } else if (
        g_automatic_selection.identity != nullptr &&
        g_lifetime_frame_serial > g_automatic_selection.last_seen_frame +
                                      kAutomaticSelectionLostGraceFrames) {
        if (g_automatic_selection.ready) {
            g_automatic_selection_notice_state = g_automatic_selection;
            g_automatic_selection.ready = false;
            g_automatic_selection_notice = AutomaticSelectionNotice::fallback;
        }
        g_automatic_selection.identity = nullptr;
        g_automatic_selection.confirmations = 0;
        ++g_automatic_selection.generation;
    }

    selection->generation = g_automatic_selection.generation;
    selection->source_width = g_automatic_selection.source_width;
    selection->source_height = g_automatic_selection.source_height;
    selection->source_format = g_automatic_selection.source_format;
    selection->output_width = g_automatic_selection.output_width;
    selection->output_height = g_automatic_selection.output_height;
    selection->confidence = g_automatic_selection.confidence;
    if (g_automatic_selection.ready) {
        selection->flags |= PHOTOREALISM_FSR_AUTOMATIC_SELECTION_READY;
    }
    const bool ready = g_automatic_selection.ready;
    ReleaseSRWLockExclusive(&g_catalog_lock);
    return ready ? S_OK : S_FALSE;
}

HRESULT WINAPI process_shader_resources(
    PhotorealismFsrShaderResourcesEventV4* event) {
    if (event == nullptr || event->struct_size < sizeof(*event) ||
        event->context == nullptr || event->view_count == 0 ||
        event->input_views == nullptr || event->output_views == nullptr ||
        event->output_capacity < event->view_count) {
        return E_INVALIDARG;
    }
    event->result_flags = 0;
    if (!TryAcquireSRWLockExclusive(&g_catalog_lock)) {
        InterlockedIncrement64(&g_lock_contention_drops);
        return S_FALSE;
    }
    if (g_device == nullptr || !g_window_active ||
        !g_current_output_is_backbuffer) {
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return S_FALSE;
    }
    if (g_current_output_context != nullptr &&
        g_current_output_context != event->context) {
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return S_FALSE;
    }

    int selected_view = -1;
    UINT selected_width = 0;
    UINT selected_height = 0;
    UINT selected_format = 0;
    for (UINT index = 0; index < event->view_count; ++index) {
        ID3D11ShaderResourceView* view = event->input_views[index];
        if (view == nullptr) {
            continue;
        }
        void* identity = nullptr;
        UINT width = 0;
        UINT height = 0;
        UINT format = 0;
        if (!resolve_shader_view_locked(
                view, &identity, &width, &height, &format)) {
            continue;
        }
        // A resource bind is only observation. It is not proof that the
        // following draw is the final full-screen composition.
        if (g_automatic_selection.ready &&
            identity == g_automatic_selection.identity &&
            width == g_automatic_selection.source_width &&
            height == g_automatic_selection.source_height &&
            photorealism::fsr::is_fsr_color_format(format)) {
            selected_view = static_cast<int>(index);
            selected_width = width;
            selected_height = height;
            selected_format = format;
        }
    }

    const AutomaticSelectionState selection = g_automatic_selection;
    const std::uint64_t frame_serial = g_lifetime_frame_serial;
    const bool runtime_available = g_fsr_runtime_available;
    ReleaseSRWLockExclusive(&g_catalog_lock);
    if (selected_view < 0 || !runtime_available) {
        return S_FALSE;
    }
    if (kSrvReplacementRequiresDrawProof) {
        bool log_blocked_replacement = false;
        AcquireSRWLockExclusive(&g_catalog_lock);
        if (g_srv_replacement_blocked_generation != selection.generation) {
            g_srv_replacement_blocked_generation = selection.generation;
            log_blocked_replacement = true;
        }
        ReleaseSRWLockExclusive(&g_catalog_lock);
        if (log_blocked_replacement) {
            log_message(
                "AA/FSR 0.7.1 seguranca: substituicao de scene-SRV "
                "bloqueada; bind sem prova do draw final (viewport/scissor/"
                "shader). Observacao continua e o core visual permanece ativo.");
        }
        return S_FALSE;
    }
    const auto policy = photorealism::fsr::evaluate_fsr_execution_policy(
        selected_width,
        selected_height,
        selection.output_width,
        selection.output_height);
    if (!TryAcquireSRWLockExclusive(&g_runtime_lock)) {
        return S_FALSE;
    }
    ID3D11ShaderResourceView* replacement = nullptr;
    bool temporal_aa_executed = false;
    bool easu_executed = false;
    bool rcas_executed = false;
    const bool executed = execute_fsr_runtime(
        event->context,
        event->input_views[static_cast<UINT>(selected_view)],
        selected_width,
        selected_height,
        selected_format,
        selection.output_width,
        selection.output_height,
        frame_serial,
        policy.execute,
        &replacement,
        &temporal_aa_executed,
        &easu_executed,
        &rcas_executed);
    ReleaseSRWLockExclusive(&g_runtime_lock);
    if (!executed || replacement == nullptr) {
        return S_FALSE;
    }
    event->output_views[static_cast<UINT>(selected_view)] = replacement;
    event->result_flags = PHOTOREALISM_FSR_SHADER_RESOURCES_REPLACED;
    if (temporal_aa_executed) {
        event->result_flags |= PHOTOREALISM_TEMPORAL_AA_EXECUTED |
                               PHOTOREALISM_SPATIAL_AA_EXECUTED;
    }
    if (easu_executed) {
        event->result_flags |= PHOTOREALISM_FSR_EASU_EXECUTED;
    }
    if (rcas_executed) {
        event->result_flags |= PHOTOREALISM_FSR_RCAS_EXECUTED;
    }
    AcquireSRWLockExclusive(&g_catalog_lock);
    if (g_fsr_logged_generation != selection.generation) {
        g_fsr_execution_notice_pending = true;
        g_fsr_logged_generation = selection.generation;
    }
    ReleaseSRWLockExclusive(&g_catalog_lock);
    return S_OK;
}

bool approximately_equal(FLOAT left, FLOAT right) {
    return std::fabs(left - right) <= 0.01f;
}

bool is_fullscreen_viewport(
    const D3D11_VIEWPORT& viewport, UINT width, UINT height) {
    return approximately_equal(viewport.TopLeftX, 0.0f) &&
           approximately_equal(viewport.TopLeftY, 0.0f) &&
           approximately_equal(viewport.Width, static_cast<FLOAT>(width)) &&
           approximately_equal(viewport.Height, static_cast<FLOAT>(height)) &&
           approximately_equal(viewport.MinDepth, 0.0f) &&
           approximately_equal(viewport.MaxDepth, 1.0f);
}

bool is_fullscreen_scissor(const D3D11_RECT& rectangle, UINT width, UINT height) {
    return rectangle.left == 0 && rectangle.top == 0 &&
           rectangle.right == static_cast<LONG>(width) &&
           rectangle.bottom == static_cast<LONG>(height);
}

bool is_fullscreen_draw_shape(const PhotorealismFsrDrawEventV5& event) {
    const bool primitive_count_valid =
        event.primitive_count == 3u || event.primitive_count == 4u ||
        event.primitive_count == 6u;
    if (!primitive_count_valid) {
        return false;
    }
    switch (event.kind) {
        case PHOTOREALISM_FSR_DRAW:
        case PHOTOREALISM_FSR_DRAW_INDEXED:
            return event.instance_count == 1u;
        case PHOTOREALISM_FSR_DRAW_INSTANCED:
        case PHOTOREALISM_FSR_DRAW_INDEXED_INSTANCED:
            return event.instance_count == 1u &&
                   event.start_instance_location == 0u;
        default:
            return false;
    }
}

bool is_fullscreen_topology(D3D11_PRIMITIVE_TOPOLOGY topology, UINT count) {
    return (topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST &&
            (count == 3u || count == 6u)) ||
           (topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP && count == 4u);
}

bool is_base_scene_candidate_locked(
    void* identity,
    UINT width,
    UINT height,
    UINT format) {
    const int resource_index = find_resource_locked(identity);
    if (resource_index < 0 || is_backbuffer_identity_locked(identity) ||
        width != g_backbuffer_width || height != g_backbuffer_height ||
        !photorealism::fsr::is_fsr_color_format(format)) {
        return false;
    }
    const ColorResourceEntry& candidate =
        g_resources[static_cast<std::size_t>(resource_index)];
    const UINT candidate_format =
        photorealism::fsr::is_fsr_color_format(candidate.texture_format)
            ? candidate.texture_format
            : candidate.primary_view_format;
    return photorealism::fsr::score_automatic_scene_candidate_base({
        candidate.width,
        candidate.height,
        candidate_format,
        candidate.sample_count,
        candidate.bind_flags,
        candidate.misc_flags,
        candidate.mip_levels,
        candidate.array_size,
        g_backbuffer_width,
        g_backbuffer_height,
        0,
        0,
        candidate.bindings,
        candidate.slot_zero_bindings,
        candidate.last_serial,
        g_binding_serial,
        candidate.last_frame,
        g_frame_serial,
        candidate.direct_composition_hits,
        candidate.last_composition_frame,
        candidate.exact_backbuffer_resource}).eligible;
}

void WINAPI observe_pixel_shader_resources(
    const PhotorealismFsrPixelShaderResourcesEventV5* event) {
    if (event == nullptr || event->struct_size < sizeof(*event) ||
        event->context == nullptr || event->views == nullptr ||
        event->view_count == 0 ||
        event->start_slot >= kPixelShaderResourceSlotCount ||
        event->view_count > kPixelShaderResourceSlotCount - event->start_slot) {
        return;
    }
    if (!TryAcquireSRWLockExclusive(&g_catalog_lock)) {
        InterlockedIncrement64(&g_lock_contention_drops);
        return;
    }
    if (g_device == nullptr || event->context != g_immediate_context) {
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return;
    }
    for (UINT index = 0; index < event->view_count; ++index) {
        g_pixel_shader_resource_shadow[event->start_slot + index] =
            event->views[index];
    }
    if (event->start_slot == 0u) {
        ID3D11ShaderResourceView* slot_zero =
            g_pixel_shader_resource_shadow[0];
        void* identity = nullptr;
        UINT width = 0;
        UINT height = 0;
        UINT format = 0;
        g_pixel_shader_slot_zero_candidate.store(
            slot_zero != nullptr &&
                resolve_shader_view_locked(
                    slot_zero, &identity, &width, &height, &format) &&
                is_base_scene_candidate_locked(identity, width, height, format),
            std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&g_catalog_lock);
}

void WINAPI observe_rasterizer_state(
    const PhotorealismFsrRasterizerStateEventV6* event) {
    if (event == nullptr || event->struct_size < sizeof(*event) ||
        event->context == nullptr) {
        return;
    }
    if (!TryAcquireSRWLockExclusive(&g_catalog_lock)) {
        InterlockedIncrement64(&g_lock_contention_drops);
        g_rasterizer_shadow_epoch.fetch_add(1u, std::memory_order_acq_rel);
        return;
    }
    if (g_device != nullptr) {
        RasterizerShadowState* shadow =
            prepare_rasterizer_shadow_locked(event->context);
        if (shadow != nullptr) {
            D3D11_RASTERIZER_DESC description = {};
            if (event->state != nullptr) {
                event->state->GetDesc(&description);
            }
            shadow->rasterizer_known = true;
            shadow->scissor_enabled = description.ScissorEnable != FALSE;
        }
    }
    ReleaseSRWLockExclusive(&g_catalog_lock);
}

void WINAPI observe_viewports(const PhotorealismFsrViewportsEventV6* event) {
    if (event == nullptr || event->struct_size < sizeof(*event) ||
        event->context == nullptr ||
        event->viewport_count >
            D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ||
        (event->viewport_count != 0 && event->viewports == nullptr)) {
        return;
    }
    if (!TryAcquireSRWLockExclusive(&g_catalog_lock)) {
        InterlockedIncrement64(&g_lock_contention_drops);
        g_rasterizer_shadow_epoch.fetch_add(1u, std::memory_order_acq_rel);
        return;
    }
    if (g_device != nullptr) {
        RasterizerShadowState* shadow =
            prepare_rasterizer_shadow_locked(event->context);
        if (shadow != nullptr) {
            shadow->viewports = {};
            shadow->viewport_count = event->viewport_count;
            shadow->viewport_known = true;
            if (event->viewport_count != 0) {
                std::memcpy(
                    shadow->viewports.data(),
                    event->viewports,
                    sizeof(D3D11_VIEWPORT) * event->viewport_count);
            }
        }
    }
    ReleaseSRWLockExclusive(&g_catalog_lock);
}

void WINAPI observe_scissor_rects(
    const PhotorealismFsrScissorRectsEventV6* event) {
    if (event == nullptr || event->struct_size < sizeof(*event) ||
        event->context == nullptr ||
        event->scissor_count >
            D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ||
        (event->scissor_count != 0 && event->scissors == nullptr)) {
        return;
    }
    if (!TryAcquireSRWLockExclusive(&g_catalog_lock)) {
        InterlockedIncrement64(&g_lock_contention_drops);
        g_rasterizer_shadow_epoch.fetch_add(1u, std::memory_order_acq_rel);
        return;
    }
    if (g_device != nullptr) {
        RasterizerShadowState* shadow =
            prepare_rasterizer_shadow_locked(event->context);
        if (shadow != nullptr) {
            shadow->scissors = {};
            shadow->scissor_count = event->scissor_count;
            shadow->scissor_known = true;
            if (event->scissor_count != 0) {
                std::memcpy(
                    shadow->scissors.data(),
                    event->scissors,
                    sizeof(D3D11_RECT) * event->scissor_count);
            }
        }
    }
    ReleaseSRWLockExclusive(&g_catalog_lock);
}

void WINAPI observe_final_draw(const PhotorealismFsrDrawEventV5* event) {
    if (event == nullptr || event->struct_size < sizeof(*event) ||
        event->context == nullptr) {
        return;
    }
    if (!g_pixel_shader_slot_zero_candidate.load(std::memory_order_acquire)) {
        return;
    }
    if (!TryAcquireSRWLockExclusive(&g_catalog_lock)) {
        InterlockedIncrement64(&g_lock_contention_drops);
        return;
    }
    ++g_draw_proof_diagnostics.observed;
    if (g_device == nullptr || !g_window_active ||
        event->context != g_immediate_context) {
        ++g_draw_proof_diagnostics.wrong_context;
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return;
    }
    if (g_pixel_shader_resource_shadow[0] == nullptr) {
        ++g_draw_proof_diagnostics.missing_shadow;
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return;
    }
    if (!g_pixel_shader_slot_zero_candidate.load(std::memory_order_acquire)) {
        ++g_draw_proof_diagnostics.source_ineligible;
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return;
    }

    const std::uint64_t draw_serial = ++g_draw_proof_draw_serial;
    RejectedDrawSignature rejected = {};
    rejected.kind = event->kind;
    rejected.primitive_count = event->primitive_count;
    rejected.instance_count = event->instance_count;
    rejected.start_location = event->start_location;
    rejected.base_vertex_location = event->base_vertex_location;
    rejected.start_instance_location = event->start_instance_location;

    const RasterizerShadowState* rasterizer_shadow =
        find_rasterizer_shadow_locked(event->context, false);
    const std::uint64_t rasterizer_epoch =
        g_rasterizer_shadow_epoch.load(std::memory_order_acquire);
    if (rasterizer_shadow != nullptr &&
        rasterizer_shadow->epoch == rasterizer_epoch) {
        rejected.rasterizer_known = rasterizer_shadow->rasterizer_known;
        rejected.viewport_known = rasterizer_shadow->viewport_known;
        rejected.scissor_known = rasterizer_shadow->scissor_known;
        rejected.scissor_enabled = rasterizer_shadow->scissor_enabled;
        rejected.viewport_count = rasterizer_shadow->viewport_count;
        rejected.scissor_count = rasterizer_shadow->scissor_count;
        rejected.viewports = rasterizer_shadow->viewports;
        rejected.scissors = rasterizer_shadow->scissors;
    }

    ID3D11RenderTargetView* render_targets[
        D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* depth_target = nullptr;
    event->context->OMGetRenderTargets(
        D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_targets, &depth_target);
    auto release_render_targets = [&]() {
        for (ID3D11RenderTargetView* target : render_targets) {
            if (target != nullptr) {
                target->Release();
            }
        }
        if (depth_target != nullptr) {
            depth_target->Release();
        }
    };
    rejected.depth_bound = depth_target != nullptr;
    rejected.depth_stencil_view = depth_target;
    for (UINT index = 0;
         index < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
         ++index) {
        if (render_targets[index] != nullptr) {
            ++rejected.rtv_count;
        }
    }
    rejected.rtv_identity = render_targets[0];

    int output_index = -1;
    if (render_targets[0] != nullptr) {
        output_index = resolve_view_locked(render_targets[0]);
        if (output_index >= 0) {
            const ColorResourceEntry& output =
                g_resources[static_cast<std::size_t>(output_index)];
            rejected.rtv_identity = output.identity;
            rejected.rtv_width = output.width;
            rejected.rtv_height = output.height;
            rejected.rtv_format = output.primary_view_format;
        }
    }

    ID3D11ShaderResourceView* source = nullptr;
    event->context->PSGetShaderResources(0, 1, &source);
    rejected.source_identity = source;
    void* source_identity = nullptr;
    UINT source_width = 0;
    UINT source_height = 0;
    UINT source_format = 0;
    const bool source_resolved =
        source != nullptr &&
        resolve_shader_view_locked(
            source,
            &source_identity,
            &source_width,
            &source_height,
            &source_format);
    if (source_resolved) {
        rejected.source_identity = source_identity;
        rejected.source_width = source_width;
        rejected.source_height = source_height;
        rejected.source_format = source_format;
    }

    ID3D11PixelShader* pixel_shader = nullptr;
    ID3D11ClassInstance* classes[D3D11_SHADER_MAX_INTERFACES] = {};
    UINT class_count = D3D11_SHADER_MAX_INTERFACES;
    event->context->PSGetShader(&pixel_shader, classes, &class_count);
    for (UINT index = 0; index < class_count; ++index) {
        if (classes[index] != nullptr) {
            classes[index]->Release();
        }
    }
    rejected.pixel_shader = pixel_shader;
    event->context->IAGetPrimitiveTopology(&rejected.topology);

    auto release_observation = [&]() {
        if (pixel_shader != nullptr) {
            pixel_shader->Release();
            pixel_shader = nullptr;
        }
        if (source != nullptr) {
            source->Release();
            source = nullptr;
        }
        release_render_targets();
    };
    auto reject = [&](DrawProofRejectReason reason) {
        rejected.reason = reason;
        record_rejected_draw_locked(rejected, draw_serial);
        release_observation();
        ReleaseSRWLockExclusive(&g_catalog_lock);
    };

    if (!is_fullscreen_draw_shape(*event)) {
        ++g_draw_proof_diagnostics.call_shape;
        reject(DrawProofRejectReason::call_shape);
        return;
    }
    if (render_targets[0] == nullptr) {
        ++g_draw_proof_diagnostics.missing_rtv;
        reject(DrawProofRejectReason::missing_rtv);
        return;
    }
    for (UINT index = 1; index < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
         ++index) {
        if (render_targets[index] != nullptr) {
            ++g_draw_proof_diagnostics.multiple_rtvs;
            reject(DrawProofRejectReason::multiple_rtvs);
            return;
        }
    }
    if (depth_target != nullptr) {
        ++g_draw_proof_diagnostics.depth_bound;
        reject(DrawProofRejectReason::depth_bound);
        return;
    }
    if (output_index < 0 ||
        !g_resources[static_cast<std::size_t>(output_index)]
             .exact_backbuffer_resource) {
        ++g_draw_proof_diagnostics.wrong_backbuffer;
        reject(DrawProofRejectReason::wrong_backbuffer);
        return;
    }
    const ColorResourceEntry& output =
        g_resources[static_cast<std::size_t>(output_index)];
    if (output.width != g_backbuffer_width ||
        output.height != g_backbuffer_height) {
        ++g_draw_proof_diagnostics.wrong_backbuffer;
        reject(DrawProofRejectReason::wrong_backbuffer);
        return;
    }
    if (source == nullptr || source != g_pixel_shader_resource_shadow[0]) {
        ++g_draw_proof_diagnostics.source_mismatch;
        reject(DrawProofRejectReason::source_mismatch);
        return;
    }
    const int source_index = source_resolved
                                 ? find_resource_locked(source_identity)
                                 : -1;
    if (source_index < 0 || is_backbuffer_identity_locked(source_identity) ||
        source_width != g_backbuffer_width ||
        source_height != g_backbuffer_height) {
        ++g_draw_proof_diagnostics.source_ineligible;
        reject(DrawProofRejectReason::source_ineligible);
        return;
    }
    const ColorResourceEntry& candidate =
        g_resources[static_cast<std::size_t>(source_index)];
    const UINT candidate_format =
        photorealism::fsr::is_fsr_color_format(candidate.texture_format)
            ? candidate.texture_format
            : candidate.primary_view_format;
    const auto candidate_score =
        photorealism::fsr::score_automatic_scene_candidate_base({
            candidate.width,
            candidate.height,
            candidate_format,
            candidate.sample_count,
            candidate.bind_flags,
            candidate.misc_flags,
            candidate.mip_levels,
            candidate.array_size,
            g_backbuffer_width,
            g_backbuffer_height,
            0,
            0,
            candidate.bindings,
            candidate.slot_zero_bindings,
            candidate.last_serial,
            g_binding_serial,
            candidate.last_frame,
            g_frame_serial,
            candidate.direct_composition_hits,
            candidate.last_composition_frame,
            candidate.exact_backbuffer_resource});
    if (!candidate_score.eligible ||
        !photorealism::fsr::is_fsr_color_format(source_format)) {
        ++g_draw_proof_diagnostics.source_ineligible;
        reject(DrawProofRejectReason::source_ineligible);
        return;
    }
    if (!rejected.rasterizer_known) {
        ++g_draw_proof_diagnostics.raster_shadow;
        reject(DrawProofRejectReason::raster_shadow);
        return;
    }
    if (!rejected.viewport_known || rejected.viewport_count != 1u ||
        !is_fullscreen_viewport(
            rejected.viewports[0], g_backbuffer_width, g_backbuffer_height)) {
        ++g_draw_proof_diagnostics.viewport;
        reject(DrawProofRejectReason::viewport);
        return;
    }
    if (rejected.scissor_enabled) {
        if (!rejected.scissor_known || rejected.scissor_count != 1u ||
            !is_fullscreen_scissor(
                rejected.scissors[0],
                g_backbuffer_width,
                g_backbuffer_height)) {
            ++g_draw_proof_diagnostics.scissor;
            reject(DrawProofRejectReason::scissor);
            return;
        }
    }
    if (pixel_shader == nullptr) {
        ++g_draw_proof_diagnostics.pixel_shader;
        reject(DrawProofRejectReason::pixel_shader);
        return;
    }
    if (!is_fullscreen_topology(rejected.topology, event->primitive_count)) {
        ++g_draw_proof_diagnostics.topology;
        reject(DrawProofRejectReason::topology);
        return;
    }

    FinalDrawProofSignature signature = {};
    signature.source_identity = source_identity;
    signature.backbuffer_identity = output.identity;
    signature.pixel_shader = pixel_shader;
    signature.topology = rejected.topology;
    signature.kind = event->kind;
    signature.primitive_count = event->primitive_count;
    signature.instance_count = event->instance_count;
    signature.start_location = event->start_location;
    signature.base_vertex_location = event->base_vertex_location;
    signature.start_instance_location = event->start_instance_location;
    signature.source_width = source_width;
    signature.source_height = source_height;
    signature.source_format = source_format;
    signature.output_width = g_backbuffer_width;
    signature.output_height = g_backbuffer_height;
    release_observation();

    if (g_final_draw_proof_last_frame == g_lifetime_frame_serial) {
        ++g_draw_proof_diagnostics.duplicate_frame;
        ReleaseSRWLockExclusive(&g_catalog_lock);
        return;
    }
    ColorResourceEntry& mutable_candidate =
        g_resources[static_cast<std::size_t>(source_index)];
    ++mutable_candidate.direct_composition_hits;
    mutable_candidate.last_composition_frame = g_frame_serial;
    ++g_draw_proof_diagnostics.valid;
    const bool same_signature =
        g_final_draw_proof_streak != 0u &&
        final_draw_signature_matches(g_final_draw_proof_signature, signature);
    g_final_draw_proof_signature = signature;
    g_final_draw_proof_last_frame = g_lifetime_frame_serial;
    g_final_draw_proof_streak = same_signature
                                     ? g_final_draw_proof_streak + 1u
                                     : 1u;
    if (!g_final_draw_proof_locked &&
        g_final_draw_proof_streak >= kFinalDrawProofConfirmFrames) {
        g_final_draw_proof_locked = true;
        log_message(
            "Final draw proof locked 0.7.1: frames=%u source=%p %ux%u "
            "output=%ux%u. replacement=0 dispatch=0; revisao passiva de "
            "draws rejeitados permanece ativa.",
            kFinalDrawProofConfirmFrames,
            signature.source_identity,
            signature.source_width,
            signature.source_height,
            signature.output_width,
            signature.output_height);
    }
    ReleaseSRWLockExclusive(&g_catalog_lock);
}

void WINAPI reset_color_observation(std::uint32_t reason) {
    AcquireSRWLockExclusive(&g_catalog_lock);
    const bool was_active = g_window_active;
    clear_catalog_locked(true);
    ReleaseSRWLockExclusive(&g_catalog_lock);
    AcquireSRWLockExclusive(&g_runtime_lock);
    discard_fsr_runtime_output();
    ReleaseSRWLockExclusive(&g_runtime_lock);
    if (was_active) {
        enqueue_reset_notice(reason);
    }
}

void WINAPI shutdown_device() {
    enqueue_color_report(ColorReportReason::device_shutdown);
    AcquireSRWLockExclusive(&g_catalog_lock);
    ID3D11Device* device = g_device;
    ID3D11DeviceContext* immediate_context = g_immediate_context;
    g_device = nullptr;
    g_immediate_context = nullptr;
    g_fsr_runtime_available = false;
    clear_catalog_locked(true);
    ReleaseSRWLockExclusive(&g_catalog_lock);

    AcquireSRWLockExclusive(&g_runtime_lock);
    shutdown_fsr_runtime();
    ReleaseSRWLockExclusive(&g_runtime_lock);

    if (device != nullptr) {
        device->Release();
    }
    if (immediate_context != nullptr) {
        immediate_context->Release();
    }
    stop_diagnostic_worker();
    if (device != nullptr) {
        log_message(
            "Photorealism FSR/AA 0.7.1: dispositivo encerrado; worker "
            "diagnostico drenado com seguranca.");
    }
}

const PhotorealismFsrApiV1 g_api_v1 = {
    sizeof(PhotorealismFsrApiV1),
    PHOTOREALISM_FSR_ABI_V1,
    PHOTOREALISM_FSR_MODULE_0_7_1,
    0,
    &initialize_device,
    &shutdown_device,
};

const PhotorealismFsrApiV2 g_api_v2 = {
    {
        sizeof(PhotorealismFsrApiV2),
        PHOTOREALISM_FSR_ABI_V2,
        PHOTOREALISM_FSR_MODULE_0_7_1,
        0,
        &initialize_device,
        &shutdown_device,
    },
    &observe_color_targets,
    &observe_frame,
    &reset_color_observation,
};

const PhotorealismFsrApiV3 g_api_v3 = {
    {
        {
            sizeof(PhotorealismFsrApiV3),
            PHOTOREALISM_FSR_ABI_V3,
            PHOTOREALISM_FSR_MODULE_0_7_1,
            0,
            &initialize_device,
            &shutdown_device,
        },
        &observe_color_targets,
        &observe_frame,
        &reset_color_observation,
    },
    &update_automatic_selection,
};

const PhotorealismFsrApiV4 g_api_v4 = {
    {
        {
            {
                sizeof(PhotorealismFsrApiV4),
                PHOTOREALISM_FSR_ABI_V4,
                PHOTOREALISM_FSR_MODULE_0_7_1,
                0,
                &initialize_device,
                &shutdown_device,
            },
            &observe_color_targets,
            &observe_frame,
            &reset_color_observation,
        },
        &update_automatic_selection,
    },
    &process_shader_resources,
};

const PhotorealismFsrApiV5 g_api_v5 = {
    {
        {
            {
                {
                    sizeof(PhotorealismFsrApiV5),
                    PHOTOREALISM_FSR_ABI_V5,
                    PHOTOREALISM_FSR_MODULE_0_7_1,
                    0,
                    &initialize_device,
                    &shutdown_device,
                },
                &observe_color_targets,
                &observe_frame,
                &reset_color_observation,
            },
            &update_automatic_selection,
        },
        &process_shader_resources,
    },
    &observe_pixel_shader_resources,
    &observe_final_draw,
};

const PhotorealismFsrApiV6 g_api_v6 = {
    {
        {
            {
                {
                    {
                        sizeof(PhotorealismFsrApiV6),
                        PHOTOREALISM_FSR_ABI_V6,
                        PHOTOREALISM_FSR_MODULE_0_7_1,
                        0,
                        &initialize_device,
                        &shutdown_device,
                    },
                    &observe_color_targets,
                    &observe_frame,
                    &reset_color_observation,
                },
                &update_automatic_selection,
            },
            &process_shader_resources,
        },
        &observe_pixel_shader_resources,
        &observe_final_draw,
    },
    &observe_rasterizer_state,
    &observe_viewports,
    &observe_scissor_rects,
};

}  // namespace

extern "C" HRESULT WINAPI PhotorealismFsrGetApi(
    std::uint32_t requested_abi,
    const PhotorealismFsrApiV1** api) {
    if (api == nullptr) {
        return E_POINTER;
    }
    *api = nullptr;
    if (requested_abi == PHOTOREALISM_FSR_ABI_V6) {
        *api = &g_api_v6.base.base.base.base.base;
        return S_OK;
    }
    if (requested_abi == PHOTOREALISM_FSR_ABI_V5) {
        *api = &g_api_v5.base.base.base.base;
        return S_OK;
    }
    if (requested_abi == PHOTOREALISM_FSR_ABI_V4) {
        *api = &g_api_v4.base.base.base;
        return S_OK;
    }
    if (requested_abi == PHOTOREALISM_FSR_ABI_V3) {
        *api = &g_api_v3.base.base;
        return S_OK;
    }
    if (requested_abi == PHOTOREALISM_FSR_ABI_V2) {
        *api = &g_api_v2.base;
        return S_OK;
    }
    if (requested_abi == PHOTOREALISM_FSR_ABI_V1) {
        *api = &g_api_v1;
        return S_OK;
    }
    return E_NOINTERFACE;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
