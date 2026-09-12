#include "fsr_telemetry.hpp"

#include "../runtime.hpp"

#include <windows.h>

namespace photorealism {
namespace fsr {
namespace {

constexpr unsigned long long kReportIntervalMs = 10000ull;

}

Telemetry& telemetry() {
    static Telemetry instance;
    return instance;
}

void Telemetry::record_replacement() {
    replacements_.fetch_add(1, std::memory_order_acq_rel);
}

void Telemetry::record_dispatch() {
    dispatches_.fetch_add(1, std::memory_order_acq_rel);
}

void Telemetry::record_skip(const char* reason) {
    skips_.fetch_add(1, std::memory_order_acq_rel);
    last_skip_ = reason;
}

void Telemetry::reset() {
    replacements_.store(0, std::memory_order_release);
    dispatches_.store(0, std::memory_order_release);
    skips_.store(0, std::memory_order_release);
    last_skip_ = nullptr;
    announced_ = false;
}

void Telemetry::report(
    const RenderExtent& internal, unsigned width, unsigned height) {
    const unsigned long long now = GetTickCount64();
    const bool due = last_report_ms_ == 0 ||
                     now - last_report_ms_ >= kReportIntervalMs;
    if (!due) {
        return;
    }
    last_report_ms_ = now;

    const unsigned replaced = replacements();
    const unsigned dispatched = dispatches();
    log_message(
        "FSR interno=%ux%u saida=%ux%u fsr.replacement=%u fsr.dispatch=%u "
        "descartes=%u motivo=%s.",
        internal.width,
        internal.height,
        width,
        height,
        replaced,
        dispatched,
        skips_.load(std::memory_order_acquire),
        last_skip_ != nullptr ? last_skip_ : "nenhum");

    if (announced_ || (replaced != 0 && dispatched != 0)) {
        announced_ = replaced != 0 && dispatched != 0;
        return;
    }
    log_message(
        "FSR ligado e sem efeito: fsr.replacement=%u fsr.dispatch=%u. Foi "
        "assim que o modulo removido na 0.15.0 viveu ate ser apagado.",
        replaced,
        dispatched);
}

}
}
