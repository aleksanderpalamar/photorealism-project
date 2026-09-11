#pragma once

#include "input_state.hpp"

#include <atomic>

namespace photorealism {
namespace overlay {

enum class PointerSource {
    None = 0,
    DeviceState = 1,
    DeviceData = 2,
};

class PointerFeed {
  public:
    void push(PointerSource source, long dx, long dy, long wheel, int buttons);
    void reset();

    bool active() const {
        return source_.load(std::memory_order_acquire) != PointerSource::None;
    }

    PointerState consume(float width, float height);

  private:
    std::atomic<PointerSource> source_{PointerSource::None};
    std::atomic<long> dx_{0};
    std::atomic<long> dy_{0};
    std::atomic<long> wheel_{0};
    std::atomic<int> buttons_{0};
    float x_ = 0.0f;
    float y_ = 0.0f;
    bool seeded_ = false;
    bool left_was_down_ = false;
};

PointerFeed& pointer_feed();

}
}
