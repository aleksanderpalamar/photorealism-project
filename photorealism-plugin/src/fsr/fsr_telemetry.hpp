#pragma once

#include "render_scale.hpp"

namespace photorealism {
namespace fsr {

class Telemetry {
  public:
    void record_replacement();
    void record_dispatch();
    void record_skip(const char* reason);
    void reset();

    void report(const RenderExtent& internal, unsigned width, unsigned height);

  private:
    unsigned window_replacements_ = 0;
    unsigned window_dispatches_ = 0;
    unsigned window_skips_ = 0;
    const char* window_reason_ = nullptr;
    unsigned long long window_start_ms_ = 0;
    unsigned long long last_report_ms_ = 0;
    bool silence_reported_ = false;
};

Telemetry& telemetry();

}
}
