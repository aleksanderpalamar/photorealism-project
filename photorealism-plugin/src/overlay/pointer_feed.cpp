#include "pointer_feed.hpp"

#include "../runtime.hpp"

namespace photorealism {
namespace overlay {
namespace {

constexpr long kWheelStep = 120;

float clamp_axis(float value, float limit) {
    if (value < 0.0f) {
        return 0.0f;
    }
    return value > limit ? limit : value;
}

}

PointerFeed& pointer_feed() {
    static PointerFeed instance;
    return instance;
}

void PointerFeed::push(
    PointerSource source, long dx, long dy, long wheel, int buttons) {
    const bool carries_input = dx != 0 || dy != 0 || wheel != 0 || buttons != 0;
    PointerSource owner = source_.load(std::memory_order_acquire);
    if (owner == PointerSource::None) {
        if (!carries_input) {
            return;
        }
        PointerSource expected = PointerSource::None;
        if (source_.compare_exchange_strong(expected, source)) {
            log_message(
                "Menu passou a ler o ponteiro do DirectInput (%s), primeiro "
                "evento dx=%ld dy=%ld botao=%d.",
                source == PointerSource::DeviceState ? "estado" : "buffer",
                dx,
                dy,
                buttons);
        }
        owner = source_.load(std::memory_order_acquire);
    }
    if (owner != source) {
        return;
    }

    buttons_.store(buttons, std::memory_order_release);
    dx_.fetch_add(dx, std::memory_order_acq_rel);
    dy_.fetch_add(dy, std::memory_order_acq_rel);
    wheel_.fetch_add(wheel, std::memory_order_acq_rel);
}

void PointerFeed::reset() {
    source_.store(PointerSource::None, std::memory_order_release);
    dx_.store(0, std::memory_order_release);
    dy_.store(0, std::memory_order_release);
    wheel_.store(0, std::memory_order_release);
    buttons_.store(0, std::memory_order_release);
    seeded_ = false;
    left_was_down_ = false;
}

PointerState PointerFeed::consume(float width, float height) {
    if (!seeded_) {
        x_ = width * 0.5f;
        y_ = height * 0.5f;
        seeded_ = true;
        dx_.store(0, std::memory_order_release);
        dy_.store(0, std::memory_order_release);
    }

    x_ = clamp_axis(
        x_ + static_cast<float>(dx_.exchange(0, std::memory_order_acq_rel)),
        width - 1.0f);
    y_ = clamp_axis(
        y_ + static_cast<float>(dy_.exchange(0, std::memory_order_acq_rel)),
        height - 1.0f);

    const bool left_down = (buttons_.load(std::memory_order_acquire) & 1) != 0;

    PointerState state;
    state.x = x_;
    state.y = y_;
    state.inside = true;
    state.down = left_down;
    state.pressed = left_down && !left_was_down_;
    state.released = !left_down && left_was_down_;
    state.wheel =
        static_cast<float>(wheel_.exchange(0, std::memory_order_acq_rel)) /
        static_cast<float>(kWheelStep);
    left_was_down_ = left_down;
    return state;
}

}
}
