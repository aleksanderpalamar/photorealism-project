#include "fsr_runtime.hpp"

#include <d3dcompiler.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cwchar>

namespace {

using CompileFromFileFunction = decltype(&D3DCompileFromFile);

template <typename T>
void release(T*& object) {
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

struct Constants {
    std::uint32_t values[5][4] = {};
};

struct TimingSlot {
    ID3D11Query* disjoint = nullptr;
    ID3D11Query* temporal_aa_start = nullptr;
    ID3D11Query* temporal_aa_end = nullptr;
    ID3D11Query* easu_start = nullptr;
    ID3D11Query* easu_end = nullptr;
    ID3D11Query* rcas_start = nullptr;
    ID3D11Query* rcas_end = nullptr;
    bool pending = false;
};

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_immediate_context = nullptr;
ID3D11ComputeShader* g_temporal_aa_shader = nullptr;
ID3D11ComputeShader* g_easu_shader = nullptr;
ID3D11ComputeShader* g_rcas_shader = nullptr;
ID3D11SamplerState* g_sampler = nullptr;
ID3D11Buffer* g_constants = nullptr;
ID3D11Texture2D* g_temporal_aa_texture[2] = {};
ID3D11ShaderResourceView* g_temporal_aa_view[2] = {};
ID3D11UnorderedAccessView* g_temporal_aa_uav[2] = {};
ID3D11Texture2D* g_easu_texture = nullptr;
ID3D11ShaderResourceView* g_easu_view = nullptr;
ID3D11UnorderedAccessView* g_easu_uav = nullptr;
ID3D11Texture2D* g_rcas_texture = nullptr;
ID3D11ShaderResourceView* g_rcas_view = nullptr;
ID3D11UnorderedAccessView* g_rcas_uav = nullptr;
UINT g_output_width = 0;
UINT g_output_height = 0;
UINT g_source_width = 0;
UINT g_source_height = 0;
bool g_fsr_resources_enabled = false;
bool g_temporal_history_valid = false;
UINT g_temporal_write_index = 0;
ID3D11ShaderResourceView* g_last_output_view = nullptr;
std::uint64_t g_last_executed_frame = UINT64_MAX;
constexpr std::size_t kTimingSlotCount = 8;
std::array<TimingSlot, kTimingSlotCount> g_timing = {};
std::size_t g_next_timing = 0;
FsrGpuTimingSummary g_timing_summary = {};

CompileFromFileFunction resolve_compiler() {
    static const CompileFromFileFunction function = [] {
        HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
        return compiler == nullptr
                   ? nullptr
                   : reinterpret_cast<CompileFromFileFunction>(
                         GetProcAddress(compiler, "D3DCompileFromFile"));
    }();
    return function;
}

bool shader_path(HMODULE module, wchar_t* path, std::size_t capacity) {
    if (module == nullptr || path == nullptr || capacity == 0 ||
        GetModuleFileNameW(module, path, static_cast<DWORD>(capacity)) == 0) {
        return false;
    }
    wchar_t* separator = std::wcsrchr(path, L'\\');
    if (separator == nullptr) {
        return false;
    }
    *separator = L'\0';
    constexpr wchar_t suffix[] =
        L"\\photorealism-plugin\\shaders\\fsr1.hlsl";
    const std::size_t used = std::wcslen(path);
    if (used + std::size(suffix) > capacity) {
        return false;
    }
    std::wmemcpy(path + used, suffix, std::size(suffix));
    return true;
}

bool compile_shader(
    ID3D11Device* device,
    const wchar_t* path,
    const char* entry,
    ID3D11ComputeShader** shader) {
    const CompileFromFileFunction compile = resolve_compiler();
    if (compile == nullptr) {
        return false;
    }
    ID3DBlob* bytecode = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT compiled = compile(
        path,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry,
        "cs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        &bytecode,
        &errors);
    release(errors);
    if (FAILED(compiled) || bytecode == nullptr) {
        release(bytecode);
        return false;
    }
    const HRESULT created = device->CreateComputeShader(
        bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, shader);
    release(bytecode);
    return SUCCEEDED(created) && *shader != nullptr;
}

void release_output_resources() {
    release(g_rcas_uav);
    release(g_rcas_view);
    release(g_rcas_texture);
    release(g_easu_uav);
    release(g_easu_view);
    release(g_easu_texture);
    for (UINT index = 0; index < 2; ++index) {
        release(g_temporal_aa_uav[index]);
        release(g_temporal_aa_view[index]);
        release(g_temporal_aa_texture[index]);
    }
    g_source_width = 0;
    g_source_height = 0;
    g_output_width = 0;
    g_output_height = 0;
    g_fsr_resources_enabled = false;
    g_temporal_history_valid = false;
    g_temporal_write_index = 0;
    g_last_output_view = nullptr;
    g_last_executed_frame = UINT64_MAX;
}

bool create_compute_texture(
    const D3D11_TEXTURE2D_DESC& description,
    ID3D11Texture2D** texture,
    ID3D11ShaderResourceView** view,
    ID3D11UnorderedAccessView** uav) {
    return SUCCEEDED(g_device->CreateTexture2D(&description, nullptr, texture)) &&
           SUCCEEDED(g_device->CreateShaderResourceView(*texture, nullptr, view)) &&
           SUCCEEDED(g_device->CreateUnorderedAccessView(*texture, nullptr, uav));
}

bool ensure_output_resources(
    UINT source_width,
    UINT source_height,
    UINT output_width,
    UINT output_height,
    bool enable_fsr) {
    if (g_temporal_aa_texture[0] != nullptr &&
        g_temporal_aa_texture[1] != nullptr && g_rcas_texture != nullptr &&
        (!enable_fsr || g_easu_texture != nullptr) &&
        g_source_width == source_width && g_source_height == source_height &&
        g_output_width == output_width && g_output_height == output_height &&
        g_fsr_resources_enabled == enable_fsr) {
        return true;
    }
    release_output_resources();
    D3D11_TEXTURE2D_DESC description = {};
    description.Width = source_width;
    description.Height = source_height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags =
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    for (UINT index = 0; index < 2; ++index) {
        if (!create_compute_texture(
                description,
                &g_temporal_aa_texture[index],
                &g_temporal_aa_view[index],
                &g_temporal_aa_uav[index])) {
            release_output_resources();
            return false;
        }
    }
    if (enable_fsr) {
        description.Width = output_width;
        description.Height = output_height;
        if (!create_compute_texture(
                description, &g_easu_texture, &g_easu_view, &g_easu_uav)) {
            release_output_resources();
            return false;
        }
    }
    description.Width = enable_fsr ? output_width : source_width;
    description.Height = enable_fsr ? output_height : source_height;
    if (!create_compute_texture(
            description, &g_rcas_texture, &g_rcas_view, &g_rcas_uav)) {
        release_output_resources();
        return false;
    }
    g_source_width = source_width;
    g_source_height = source_height;
    g_output_width = output_width;
    g_output_height = output_height;
    g_fsr_resources_enabled = enable_fsr;
    return true;
}

std::uint32_t float_bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

Constants easu_constants(
    UINT source_width,
    UINT source_height,
    UINT source_format,
    UINT output_width,
    UINT output_height) {
    Constants constants = {};
    const float source_x = static_cast<float>(source_width);
    const float source_y = static_cast<float>(source_height);
    const float output_x = static_cast<float>(output_width);
    const float output_y = static_cast<float>(output_height);
    constants.values[0][0] = float_bits(source_x / output_x);
    constants.values[0][1] = float_bits(source_y / output_y);
    constants.values[0][2] = float_bits(0.5f * source_x / output_x - 0.5f);
    constants.values[0][3] = float_bits(0.5f * source_y / output_y - 0.5f);
    constants.values[1][0] = float_bits(1.0f / source_x);
    constants.values[1][1] = float_bits(1.0f / source_y);
    constants.values[1][2] = float_bits(1.0f / source_x);
    constants.values[1][3] = float_bits(-1.0f / source_y);
    constants.values[2][0] = float_bits(-1.0f / source_x);
    constants.values[2][1] = float_bits(2.0f / source_y);
    constants.values[2][2] = float_bits(1.0f / source_x);
    constants.values[2][3] = float_bits(2.0f / source_y);
    constants.values[3][0] = float_bits(0.0f);
    constants.values[3][1] = float_bits(4.0f / source_y);
    constants.values[4][0] =
        source_format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
                source_format == DXGI_FORMAT_R11G11B10_FLOAT
            ? 1u
            : 0u;
    constants.values[4][2] = output_width;
    constants.values[4][3] = output_height;
    return constants;
}

Constants rcas_constants(
    UINT source_format,
    UINT output_width,
    UINT output_height,
    bool input_needs_srtm) {
    Constants constants = {};
    // Conservative 0.4-stop RCAS sharpening.
    constants.values[0][0] = float_bits(std::exp2(-0.4f));
    constants.values[4][0] =
        source_format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
                source_format == DXGI_FORMAT_R11G11B10_FLOAT
            ? 1u
            : 0u;
    constants.values[4][1] = input_needs_srtm ? 1u : 0u;
    constants.values[4][2] = output_width;
    constants.values[4][3] = output_height;
    return constants;
}

Constants temporal_aa_constants(
    UINT source_width,
    UINT source_height,
    UINT source_format,
    bool history_valid) {
    Constants constants = {};
    constants.values[0][0] =
        float_bits(1.0f / static_cast<float>(source_width));
    constants.values[0][1] =
        float_bits(1.0f / static_cast<float>(source_height));
    constants.values[4][0] =
        source_format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
                source_format == DXGI_FORMAT_R11G11B10_FLOAT
            ? 1u
            : 0u;
    constants.values[4][1] = history_valid ? 1u : 0u;
    constants.values[4][2] = source_width;
    constants.values[4][3] = source_height;
    return constants;
}

bool begin_timing(ID3D11DeviceContext* context, TimingSlot** output) {
    TimingSlot& slot = g_timing[g_next_timing];
    if (slot.pending) {
        ++g_timing_summary.dropped;
        return false;
    }
    context->Begin(slot.disjoint);
    context->End(slot.temporal_aa_start);
    slot.pending = true;
    *output = &slot;
    g_next_timing = (g_next_timing + 1) % kTimingSlotCount;
    return true;
}

}  // namespace

bool initialize_fsr_runtime(ID3D11Device* device, HMODULE module) {
    shutdown_fsr_runtime();
    if (device == nullptr || device->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0) {
        return false;
    }
    UINT format_support = 0;
    if (FAILED(device->CheckFormatSupport(
            DXGI_FORMAT_R16G16B16A16_FLOAT, &format_support)) ||
        (format_support & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) == 0 ||
        (format_support &
         D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) == 0) {
        return false;
    }
    wchar_t path[MAX_PATH] = {};
    if (!shader_path(module, path, std::size(path)) ||
        !compile_shader(device, path, "CSTemporalAa", &g_temporal_aa_shader) ||
        !compile_shader(device, path, "CSEasu", &g_easu_shader) ||
        !compile_shader(device, path, "CSRcas", &g_rcas_shader)) {
        shutdown_fsr_runtime();
        return false;
    }
    D3D11_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    D3D11_BUFFER_DESC buffer = {};
    buffer.ByteWidth = sizeof(Constants);
    buffer.Usage = D3D11_USAGE_DEFAULT;
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(device->CreateSamplerState(&sampler, &g_sampler)) ||
        FAILED(device->CreateBuffer(&buffer, nullptr, &g_constants))) {
        shutdown_fsr_runtime();
        return false;
    }
    D3D11_QUERY_DESC disjoint = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
    D3D11_QUERY_DESC timestamp = {D3D11_QUERY_TIMESTAMP, 0};
    for (TimingSlot& slot : g_timing) {
        if (FAILED(device->CreateQuery(&disjoint, &slot.disjoint)) ||
            FAILED(device->CreateQuery(&timestamp, &slot.temporal_aa_start)) ||
            FAILED(device->CreateQuery(&timestamp, &slot.temporal_aa_end)) ||
            FAILED(device->CreateQuery(&timestamp, &slot.easu_start)) ||
            FAILED(device->CreateQuery(&timestamp, &slot.easu_end)) ||
            FAILED(device->CreateQuery(&timestamp, &slot.rcas_start)) ||
            FAILED(device->CreateQuery(&timestamp, &slot.rcas_end))) {
            shutdown_fsr_runtime();
            return false;
        }
    }
    device->AddRef();
    g_device = device;
    device->GetImmediateContext(&g_immediate_context);
    if (g_immediate_context == nullptr) {
        shutdown_fsr_runtime();
        return false;
    }
    return true;
}

void shutdown_fsr_runtime() {
    release_output_resources();
    for (TimingSlot& slot : g_timing) {
        release(slot.rcas_end);
        release(slot.rcas_start);
        release(slot.easu_end);
        release(slot.easu_start);
        release(slot.temporal_aa_end);
        release(slot.temporal_aa_start);
        release(slot.disjoint);
        slot.pending = false;
    }
    release(g_constants);
    release(g_sampler);
    release(g_rcas_shader);
    release(g_easu_shader);
    release(g_temporal_aa_shader);
    release(g_immediate_context);
    release(g_device);
    g_next_timing = 0;
    g_timing_summary = {};
}

bool execute_fsr_runtime(
    ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* source,
    UINT source_width,
    UINT source_height,
    UINT source_format,
    UINT output_width,
    UINT output_height,
    std::uint64_t frame_serial,
    bool enable_fsr,
    ID3D11ShaderResourceView** output,
    bool* temporal_aa_executed,
    bool* easu_executed,
    bool* rcas_executed) {
    if (output == nullptr || temporal_aa_executed == nullptr ||
        easu_executed == nullptr || rcas_executed == nullptr) {
        return false;
    }
    *output = nullptr;
    *temporal_aa_executed = false;
    *easu_executed = false;
    *rcas_executed = false;
    if (context == nullptr || source == nullptr || g_device == nullptr ||
        g_temporal_aa_shader == nullptr || g_easu_shader == nullptr ||
        g_rcas_shader == nullptr || g_temporal_aa_texture[0] == nullptr ||
        g_temporal_aa_texture[1] == nullptr || g_rcas_texture == nullptr ||
        (enable_fsr && g_easu_texture == nullptr) ||
        g_source_width != source_width || g_source_height != source_height ||
        g_output_width != output_width || g_output_height != output_height ||
        g_fsr_resources_enabled != enable_fsr) {
        return false;
    }
    if (g_last_executed_frame == frame_serial) {
        *output = g_last_output_view;
        *temporal_aa_executed = true;
        *easu_executed = enable_fsr;
        *rcas_executed = true;
        return true;
    }

    ID3D11ComputeShader* saved_shader = nullptr;
    ID3D11ClassInstance* saved_classes[D3D11_SHADER_MAX_INTERFACES] = {};
    UINT saved_class_count = D3D11_SHADER_MAX_INTERFACES;
    ID3D11ShaderResourceView* saved_srvs[2] = {};
    ID3D11UnorderedAccessView* saved_uav = nullptr;
    ID3D11Buffer* saved_buffer = nullptr;
    ID3D11SamplerState* saved_sampler = nullptr;
    context->CSGetShader(
        &saved_shader, saved_classes, &saved_class_count);
    context->CSGetShaderResources(0, 2, saved_srvs);
    context->CSGetUnorderedAccessViews(0, 1, &saved_uav);
    context->CSGetConstantBuffers(0, 1, &saved_buffer);
    context->CSGetSamplers(0, 1, &saved_sampler);

    TimingSlot* timing = nullptr;
    const bool timing_active = begin_timing(context, &timing);
    const UINT write_index = g_temporal_write_index;
    const UINT history_index = 1u - write_index;
    const Constants temporal = temporal_aa_constants(
        source_width, source_height, source_format, g_temporal_history_valid);
    context->UpdateSubresource(g_constants, 0, nullptr, &temporal, 0, 0);
    context->CSSetShader(g_temporal_aa_shader, nullptr, 0);
    ID3D11ShaderResourceView* temporal_inputs[2] = {
        source,
        g_temporal_history_valid ? g_temporal_aa_view[history_index] : nullptr,
    };
    context->CSSetShaderResources(0, 2, temporal_inputs);
    context->CSSetUnorderedAccessViews(
        0, 1, &g_temporal_aa_uav[write_index], nullptr);
    context->CSSetConstantBuffers(0, 1, &g_constants);
    context->CSSetSamplers(0, 1, &g_sampler);
    context->Dispatch((source_width + 7u) / 8u, (source_height + 7u) / 8u, 1);
    if (timing_active) {
        context->End(timing->temporal_aa_end);
        context->End(timing->easu_start);
    }

    ID3D11ShaderResourceView* null_srvs[2] = {};
    ID3D11UnorderedAccessView* null_uav = nullptr;
    context->CSSetShaderResources(0, 2, null_srvs);
    context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
    ID3D11ShaderResourceView* rcas_input = g_temporal_aa_view[write_index];
    UINT rcas_width = source_width;
    UINT rcas_height = source_height;
    if (enable_fsr) {
        const Constants easu = easu_constants(
            source_width,
            source_height,
            source_format,
            output_width,
            output_height);
        context->UpdateSubresource(g_constants, 0, nullptr, &easu, 0, 0);
        context->CSSetShader(g_easu_shader, nullptr, 0);
        context->CSSetShaderResources(0, 1, &rcas_input);
        context->CSSetUnorderedAccessViews(0, 1, &g_easu_uav, nullptr);
        context->Dispatch(
            (output_width + 15u) / 16u, (output_height + 15u) / 16u, 1);
        context->CSSetShaderResources(0, 2, null_srvs);
        context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
        rcas_input = g_easu_view;
        rcas_width = output_width;
        rcas_height = output_height;
    }
    if (timing_active) {
        context->End(timing->easu_end);
        context->End(timing->rcas_start);
    }
    const Constants rcas =
        rcas_constants(
            source_format, rcas_width, rcas_height, !enable_fsr);
    context->UpdateSubresource(g_constants, 0, nullptr, &rcas, 0, 0);
    context->CSSetShader(g_rcas_shader, nullptr, 0);
    context->CSSetShaderResources(0, 1, &rcas_input);
    context->CSSetUnorderedAccessViews(0, 1, &g_rcas_uav, nullptr);
    context->Dispatch((rcas_width + 15u) / 16u, (rcas_height + 15u) / 16u, 1);
    if (timing_active) {
        context->End(timing->rcas_end);
        context->End(timing->disjoint);
    }

    context->CSSetShaderResources(0, 2, null_srvs);
    context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
    context->CSSetShader(saved_shader, saved_classes, saved_class_count);
    context->CSSetShaderResources(0, 2, saved_srvs);
    context->CSSetUnorderedAccessViews(0, 1, &saved_uav, nullptr);
    context->CSSetConstantBuffers(0, 1, &saved_buffer);
    context->CSSetSamplers(0, 1, &saved_sampler);
    release(saved_sampler);
    release(saved_buffer);
    release(saved_uav);
    release(saved_srvs[1]);
    release(saved_srvs[0]);
    release(saved_shader);
    for (UINT index = 0; index < saved_class_count; ++index) {
        release(saved_classes[index]);
    }

    g_temporal_history_valid = true;
    g_temporal_write_index = history_index;
    g_last_executed_frame = frame_serial;
    g_last_output_view = g_rcas_view;
    *output = g_last_output_view;
    *temporal_aa_executed = true;
    *easu_executed = enable_fsr;
    *rcas_executed = true;
    return true;
}

bool prepare_fsr_runtime_output(
    UINT source_width,
    UINT source_height,
    UINT output_width,
    UINT output_height,
    bool enable_fsr) {
    return g_device != nullptr && source_width != 0 && source_height != 0 &&
           output_width != 0 && output_height != 0 &&
           ensure_output_resources(
               source_width,
               source_height,
               output_width,
               output_height,
               enable_fsr);
}

void discard_fsr_runtime_output() {
    release_output_resources();
}

void poll_fsr_gpu_timing() {
    if (g_device == nullptr) {
        return;
    }
    ID3D11DeviceContext* context = g_immediate_context;
    if (context == nullptr) {
        return;
    }
    for (TimingSlot& slot : g_timing) {
        if (!slot.pending) {
            continue;
        }
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT data = {};
        if (context->GetData(slot.disjoint, &data, sizeof(data), 0) != S_OK) {
            continue;
        }
        UINT64 easu_start = 0;
        UINT64 easu_end = 0;
        UINT64 rcas_start = 0;
        UINT64 rcas_end = 0;
        UINT64 temporal_aa_start = 0;
        UINT64 temporal_aa_end = 0;
        const bool complete =
            context->GetData(
                slot.temporal_aa_start,
                &temporal_aa_start,
                sizeof(temporal_aa_start),
                0) == S_OK &&
            context->GetData(
                slot.temporal_aa_end,
                &temporal_aa_end,
                sizeof(temporal_aa_end),
                0) == S_OK &&
            context->GetData(slot.easu_start, &easu_start, sizeof(easu_start), 0) == S_OK &&
            context->GetData(slot.easu_end, &easu_end, sizeof(easu_end), 0) == S_OK &&
            context->GetData(slot.rcas_start, &rcas_start, sizeof(rcas_start), 0) == S_OK &&
            context->GetData(slot.rcas_end, &rcas_end, sizeof(rcas_end), 0) == S_OK;
        if (complete && !data.Disjoint && data.Frequency != 0) {
            const double easu_ms =
                static_cast<double>(easu_end - easu_start) * 1000.0 /
                static_cast<double>(data.Frequency);
            const double rcas_ms =
                static_cast<double>(rcas_end - rcas_start) * 1000.0 /
                static_cast<double>(data.Frequency);
            const double temporal_aa_ms =
                static_cast<double>(temporal_aa_end - temporal_aa_start) *
                1000.0 / static_cast<double>(data.Frequency);
            ++g_timing_summary.samples;
            g_timing_summary.temporal_aa_sum_ms += temporal_aa_ms;
            g_timing_summary.easu_sum_ms += easu_ms;
            g_timing_summary.rcas_sum_ms += rcas_ms;
            g_timing_summary.easu_max_ms =
                std::max(g_timing_summary.easu_max_ms, easu_ms);
            g_timing_summary.rcas_max_ms =
                std::max(g_timing_summary.rcas_max_ms, rcas_ms);
            g_timing_summary.temporal_aa_max_ms = std::max(
                g_timing_summary.temporal_aa_max_ms, temporal_aa_ms);
        } else {
            ++g_timing_summary.dropped;
        }
        slot.pending = false;
    }
}

FsrGpuTimingSummary take_fsr_gpu_timing_summary() {
    const FsrGpuTimingSummary summary = g_timing_summary;
    g_timing_summary = {};
    return summary;
}
