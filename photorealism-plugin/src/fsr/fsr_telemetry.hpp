#pragma once

#include "render_scale.hpp"

#include <atomic>

namespace photorealism {
namespace fsr {

class Telemetry {
  public:
    void record_replacement();
    void record_dispatch();
    void record_skip(const char* reason);
    void record_game_acquire();
    void reset();

    void report(const RenderExtent& internal, unsigned width, unsigned height);

    unsigned replacements() const {
        return replacements_.load(std::memory_order_acquire);
    }
    unsigned dispatches() const {
        return dispatches_.load(std::memory_order_acquire);
    }

  private:
    std::atomic<unsigned> replacements_{0};
    std::atomic<unsigned> dispatches_{0};
    std::atomic<unsigned> skips_{0};
    std::atomic<unsigned> game_acquires_{0};
    const char* last_skip_ = nullptr;
    unsigned long long last_report_ms_ = 0;
    bool announced_ = false;
};

Telemetry& telemetry();

}
}
