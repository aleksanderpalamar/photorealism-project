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
constexpr std::uint32_t PHOTOREALISM_FSR_MODULE_0_2_0 = 0x00000200u;
constexpr std::uint32_t PHOTOREALISM_FSR_MODULE_0_3_0 = 0x00000300u;
constexpr std::uint32_t PHOTOREALISM_FSR_MODULE_0_5_0 = 0x00000500u;
constexpr std::uint32_t PHOTOREALISM_FSR_MODULE_0_6_0 = 0x00000600u;

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

using PhotorealismFsrGetApiFunction = HRESULT(WINAPI*)(
    std::uint32_t requested_abi,
    const PhotorealismFsrApiV1** api);
static_assert(
    sizeof(PhotorealismFsrColorTargetsEventV4) == 32,
    "Photorealism FSR color-context event ABI changed unexpectedly");
