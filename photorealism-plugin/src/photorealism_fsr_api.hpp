#pragma once

#include <d3d11.h>
#include <windows.h>

#include <cstdint>

// Stable C ABI between the DXGI core and the optional FSR module.  The ABI
// version is independent from both the core plugin and module release numbers.
constexpr std::uint32_t PHOTOREALISM_FSR_ABI_V1 = 0x00010000u;
constexpr std::uint32_t PHOTOREALISM_FSR_ABI_V2 = 0x00020000u;
constexpr std::uint32_t PHOTOREALISM_FSR_ABI_V3 = 0x00030000u;
constexpr std::uint32_t PHOTOREALISM_FSR_ABI_V4 = 0x00040000u;
constexpr std::uint32_t PHOTOREALISM_FSR_ABI_V5 = 0x00050000u;
constexpr std::uint32_t PHOTOREALISM_FSR_ABI_V6 = 0x00060000u;
constexpr std::uint32_t PHOTOREALISM_FSR_MODULE_0_2_0 = 0x00000200u;
constexpr std::uint32_t PHOTOREALISM_FSR_MODULE_0_3_0 = 0x00000300u;
constexpr std::uint32_t PHOTOREALISM_FSR_MODULE_0_5_0 = 0x00000500u;
constexpr std::uint32_t PHOTOREALISM_FSR_MODULE_0_6_0 = 0x00000600u;
constexpr std::uint32_t PHOTOREALISM_FSR_MODULE_0_6_1 = 0x00000601u;
constexpr std::uint32_t PHOTOREALISM_FSR_MODULE_0_7_0 = 0x00000700u;
constexpr std::uint32_t PHOTOREALISM_FSR_MODULE_0_7_1 = 0x00000701u;

struct PhotorealismFsrApiV1 {
    std::uint32_t struct_size;
    std::uint32_t abi_version;
    std::uint32_t module_version;
    std::uint32_t reserved;
    HRESULT(WINAPI* initialize_device)(ID3D11Device* device);
    void(WINAPI* shutdown_device)();
};

static_assert(sizeof(void*) == 8, "Photorealism FSR supports Windows x64");
static_assert(
    sizeof(PhotorealismFsrApiV1) == 32,
    "Photorealism FSR ABI v1 layout changed unexpectedly");

enum PhotorealismFsrColorEventFlags : std::uint32_t {
    PHOTOREALISM_FSR_COLOR_EVENT_OM_SET_RENDER_TARGETS = 0x1u,
    PHOTOREALISM_FSR_COLOR_EVENT_OM_SET_RENDER_TARGETS_AND_UAVS = 0x2u,
};

enum PhotorealismFsrResetReason : std::uint32_t {
    PHOTOREALISM_FSR_RESET_RESIZE = 1u,
    PHOTOREALISM_FSR_RESET_PLUGIN_DISABLED = 2u,
};

struct PhotorealismFsrColorTargetsEventV2 {
    std::uint32_t struct_size;
    std::uint32_t flags;
    std::uint32_t render_target_count;
    std::uint32_t reserved;
    ID3D11RenderTargetView* const* render_targets;
};

struct PhotorealismFsrFrameEventV2 {
    std::uint32_t struct_size;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t format;
    std::uint32_t sample_count;
    std::uint32_t reserved;
    ID3D11Texture2D* back_buffer;
};

struct PhotorealismFsrApiV2 {
    // The v1 prefix remains layout-compatible.  A v1 host can request the
    // dedicated v1 table while a v2 host negotiates these diagnostics.
    PhotorealismFsrApiV1 base;
    void(WINAPI* observe_color_targets)(
        const PhotorealismFsrColorTargetsEventV2* event);
    void(WINAPI* observe_frame)(const PhotorealismFsrFrameEventV2* event);
    void(WINAPI* reset_color_observation)(std::uint32_t reason);
};

static_assert(
    sizeof(PhotorealismFsrColorTargetsEventV2) == 24,
    "Photorealism FSR color event ABI changed unexpectedly");
static_assert(
    sizeof(PhotorealismFsrFrameEventV2) == 32,
    "Photorealism FSR frame event ABI changed unexpectedly");
static_assert(
    sizeof(PhotorealismFsrApiV2) == 56,
    "Photorealism FSR ABI v2 layout changed unexpectedly");

enum PhotorealismFsrAutomaticSelectionFlagsV3 : std::uint32_t {
    PHOTOREALISM_FSR_AUTOMATIC_SELECTION_READY = 0x1u,
};

struct PhotorealismFsrSelectionContextV3 {
    std::uint32_t struct_size;
    std::uint32_t backbuffer_width;
    std::uint32_t backbuffer_height;
    std::uint32_t backbuffer_format;
    std::uint32_t depth_width;
    std::uint32_t depth_height;
    std::uint64_t depth_generation;
};

struct PhotorealismFsrAutomaticSelectionV3 {
    std::uint32_t struct_size;
    std::uint32_t flags;
    std::uint64_t generation;
    std::uint32_t source_width;
    std::uint32_t source_height;
    std::uint32_t source_format;
    std::uint32_t output_width;
    std::uint32_t output_height;
    std::uint32_t confidence;
};

struct PhotorealismFsrApiV3 {
    // v1 and v2 remain an exact prefix for backwards-compatible negotiation.
    PhotorealismFsrApiV2 base;
    HRESULT(WINAPI* update_automatic_selection)(
        const PhotorealismFsrSelectionContextV3* context,
        PhotorealismFsrAutomaticSelectionV3* selection);
};

static_assert(
    sizeof(PhotorealismFsrSelectionContextV3) == 32,
    "Photorealism FSR selection context ABI changed unexpectedly");
static_assert(
    sizeof(PhotorealismFsrAutomaticSelectionV3) == 40,
    "Photorealism FSR automatic selection ABI changed unexpectedly");
static_assert(
    sizeof(PhotorealismFsrApiV3) == 64,
    "Photorealism FSR ABI v3 layout changed unexpectedly");

enum PhotorealismFsrShaderResourceResultFlagsV4 : std::uint32_t {
    PHOTOREALISM_FSR_SHADER_RESOURCES_REPLACED = 0x1u,
    PHOTOREALISM_FSR_EASU_EXECUTED = 0x2u,
    PHOTOREALISM_FSR_RCAS_EXECUTED = 0x4u,
    PHOTOREALISM_TEMPORAL_AA_EXECUTED = 0x8u,
    PHOTOREALISM_SPATIAL_AA_EXECUTED = 0x10u,
};

struct PhotorealismFsrShaderResourcesEventV4 {
    std::uint32_t struct_size;
    std::uint32_t start_slot;
    std::uint32_t view_count;
    std::uint32_t output_capacity;
    ID3D11DeviceContext* context;
    ID3D11ShaderResourceView* const* input_views;
    ID3D11ShaderResourceView** output_views;
    std::uint32_t result_flags;
    std::uint32_t reserved;
};

struct PhotorealismFsrColorTargetsEventV4 {
    PhotorealismFsrColorTargetsEventV2 base;
    ID3D11DeviceContext* context;
};

struct PhotorealismFsrApiV4 {
    // v1/v2/v3 stay an exact prefix. The host owns the output pointer array;
    // returned SRVs remain owned by the module and are valid until reset.
    PhotorealismFsrApiV3 base;
    HRESULT(WINAPI* process_shader_resources)(
        PhotorealismFsrShaderResourcesEventV4* event);
};

static_assert(
    sizeof(PhotorealismFsrShaderResourcesEventV4) == 48,
    "Photorealism FSR shader-resource event ABI changed unexpectedly");
static_assert(
    sizeof(PhotorealismFsrApiV4) == 72,
    "Photorealism FSR ABI v4 layout changed unexpectedly");

// ABI v5 deliberately separates observation from replacement. A shader
// resource bind is cached as a low-cost hint, while the module validates the
// live D3D11 state immediately before the game's Draw call. No COM pointer
// passed through these events is retained by the module.
enum PhotorealismFsrDrawKindV5 : std::uint32_t {
    PHOTOREALISM_FSR_DRAW = 1u,
    PHOTOREALISM_FSR_DRAW_INDEXED = 2u,
    PHOTOREALISM_FSR_DRAW_INSTANCED = 3u,
    PHOTOREALISM_FSR_DRAW_INDEXED_INSTANCED = 4u,
};

struct PhotorealismFsrPixelShaderResourcesEventV5 {
    std::uint32_t struct_size;
    std::uint32_t start_slot;
    std::uint32_t view_count;
    std::uint32_t reserved;
    ID3D11DeviceContext* context;
    ID3D11ShaderResourceView* const* views;
};

struct PhotorealismFsrDrawEventV5 {
    std::uint32_t struct_size;
    std::uint32_t kind;
    std::uint32_t primitive_count;
    std::uint32_t instance_count;
    std::uint32_t start_location;
    std::int32_t base_vertex_location;
    std::uint32_t start_instance_location;
    std::uint32_t reserved;
    ID3D11DeviceContext* context;
};

struct PhotorealismFsrApiV5 {
    // v1 through v4 remain an exact prefix. v5 is observation-only in 0.7.0.
    PhotorealismFsrApiV4 base;
    void(WINAPI* observe_pixel_shader_resources)(
        const PhotorealismFsrPixelShaderResourcesEventV5* event);
    void(WINAPI* observe_final_draw)(const PhotorealismFsrDrawEventV5* event);
};

static_assert(
    sizeof(PhotorealismFsrPixelShaderResourcesEventV5) == 32,
    "Photorealism FSR ABI v5 PS event layout changed unexpectedly");
static_assert(
    sizeof(PhotorealismFsrDrawEventV5) == 40,
    "Photorealism FSR ABI v5 draw event layout changed unexpectedly");
static_assert(
    sizeof(PhotorealismFsrApiV5) == 88,
    "Photorealism FSR ABI v5 layout changed unexpectedly");

// ABI v6 adds a passive rasterizer-state shadow. The three events are emitted
// only from their matching D3D11 setters; the module copies their values and
// never retains COM interfaces or changes pipeline state.
struct PhotorealismFsrRasterizerStateEventV6 {
    std::uint32_t struct_size;
    std::uint32_t reserved;
    ID3D11DeviceContext* context;
    ID3D11RasterizerState* state;
};

struct PhotorealismFsrViewportsEventV6 {
    std::uint32_t struct_size;
    std::uint32_t viewport_count;
    ID3D11DeviceContext* context;
    const D3D11_VIEWPORT* viewports;
};

struct PhotorealismFsrScissorRectsEventV6 {
    std::uint32_t struct_size;
    std::uint32_t scissor_count;
    ID3D11DeviceContext* context;
    const D3D11_RECT* scissors;
};

struct PhotorealismFsrApiV6 {
    // v1 through v5 remain an exact prefix.
    PhotorealismFsrApiV5 base;
    void(WINAPI* observe_rasterizer_state)(
        const PhotorealismFsrRasterizerStateEventV6* event);
    void(WINAPI* observe_viewports)(
        const PhotorealismFsrViewportsEventV6* event);
    void(WINAPI* observe_scissor_rects)(
        const PhotorealismFsrScissorRectsEventV6* event);
};

static_assert(
    sizeof(PhotorealismFsrRasterizerStateEventV6) == 24,
    "Photorealism FSR ABI v6 rasterizer-state event layout changed unexpectedly");
static_assert(
    sizeof(PhotorealismFsrViewportsEventV6) == 24,
    "Photorealism FSR ABI v6 viewport event layout changed unexpectedly");
static_assert(
    sizeof(PhotorealismFsrScissorRectsEventV6) == 24,
    "Photorealism FSR ABI v6 scissor event layout changed unexpectedly");
static_assert(
    sizeof(PhotorealismFsrApiV6) == 112,
    "Photorealism FSR ABI v6 layout changed unexpectedly");

using PhotorealismFsrGetApiFunction = HRESULT(WINAPI*)(
    std::uint32_t requested_abi,
    const PhotorealismFsrApiV1** api);
static_assert(
    sizeof(PhotorealismFsrColorTargetsEventV4) == 32,
    "Photorealism FSR color-context event ABI changed unexpectedly");
