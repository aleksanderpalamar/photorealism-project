#pragma once

#include <d3d11.h>

#include <cfloat>
#include <cstdint>

namespace photorealism {

struct GpuTimingSlot {
    ID3D11Query* disjoint = nullptr;
    ID3D11Query* start = nullptr;
    ID3D11Query* end = nullptr;
    bool pending = false;
};

class GpuTimer {
  public:
    void attach(ID3D11Device* device, ID3D11DeviceContext* context);
    void release();

    bool begin();
    void end();
    void poll();

  private:
    void handle_query_failure(HRESULT result);
    void record(double milliseconds);

    static constexpr UINT kGpuTimingSlotCount = 8;
    static constexpr ULONGLONG kGpuReportIntervalMilliseconds = 10000;

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
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

}  // namespace photorealism
