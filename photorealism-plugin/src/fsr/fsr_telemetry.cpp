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
    ++window_replacements_;
}

void Telemetry::record_dispatch() {
    ++window_dispatches_;
}

void Telemetry::record_skip(const char* reason) {
    ++window_skips_;
    window_reason_ = reason;
}

void Telemetry::reset() {
    window_replacements_ = 0;
    window_dispatches_ = 0;
    window_skips_ = 0;
    window_reason_ = nullptr;
    window_start_ms_ = 0;
    last_report_ms_ = 0;
    silence_reported_ = false;
}

void Telemetry::report(
    const RenderExtent& internal, unsigned width, unsigned height) {
    const unsigned long long now = GetTickCount64();
    if (window_start_ms_ == 0) {
        window_start_ms_ = now;
    }
    const bool due = last_report_ms_ == 0 ||
                     now - last_report_ms_ >= kReportIntervalMs;
    if (!due) {
        return;
    }
    const double seconds =
        static_cast<double>(now - window_start_ms_) / 1000.0;
    log_message(
        "FSR em %.1f s: interno=%ux%u saida=%ux%u fsr.replacement=%u "
        "fsr.dispatch=%u sem_reconstrucao=%u motivo=%s.",
        seconds,
        internal.width,
        internal.height,
        width,
        height,
        window_replacements_,
        window_dispatches_,
        window_skips_,
        window_reason_ != nullptr ? window_reason_ : "nenhum");

    const bool silent = window_replacements_ == 0;
    if (silent && !silence_reported_) {
        log_message(
            "FSR ligado e sem efeito: nenhum quadro reconstruido em %.1f s. "
            "Foi assim que o modulo removido na 0.15.0 viveu ate ser apagado.",
            seconds);
    }
    silence_reported_ = silent;

    window_replacements_ = 0;
    window_dispatches_ = 0;
    window_skips_ = 0;
    window_reason_ = nullptr;
    window_start_ms_ = now;
    last_report_ms_ = now;
}

}
}
