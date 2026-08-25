#pragma once

#include <d3d11.h>
#include <windows.h>

#include <cstdint>

struct FsrGpuTimingSummary {
    std::uint64_t samples = 0;
    std::uint64_t dropped = 0;
    double easu_sum_ms = 0.0;
    double rcas_sum_ms = 0.0;
    double temporal_aa_sum_ms = 0.0;
    double easu_max_ms = 0.0;
    double rcas_max_ms = 0.0;
    double temporal_aa_max_ms = 0.0;
};

bool initialize_fsr_runtime(ID3D11Device* device, HMODULE module);
void shutdown_fsr_runtime();
bool prepare_fsr_runtime_output(
    UINT source_width,
    UINT source_height,
    UINT output_width,
    UINT output_height,
    bool enable_fsr);
void discard_fsr_runtime_output();
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
    bool* rcas_executed);
void poll_fsr_gpu_timing();
FsrGpuTimingSummary take_fsr_gpu_timing_summary();
