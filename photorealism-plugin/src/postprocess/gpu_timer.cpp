#include "gpu_timer.hpp"

#include "../runtime.hpp"
#include "com_utils.hpp"

#include <windows.h>

namespace photorealism {

void GpuTimer::attach(
    ID3D11Device* device, ID3D11DeviceContext* context) {
    release();
    device_ = device;
    context_ = context;
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
            release();
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

void GpuTimer::release() {
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

void GpuTimer::poll() {
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
            handle_query_failure(result);
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
            handle_query_failure(
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
        record(milliseconds);
    }
}

bool GpuTimer::begin() {
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

void GpuTimer::end() {
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

void GpuTimer::handle_query_failure(HRESULT result) {
    if (!gpu_query_error_logged_) {
        log_message(
            "Falha ao consultar telemetria GPU: 0x%08X; "
            "o passe visual continuara ativo.",
            static_cast<unsigned>(result));
        gpu_query_error_logged_ = true;
    }
    ++gpu_dropped_sample_count_;
}

void GpuTimer::record(double milliseconds) {
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

}  // namespace photorealism
