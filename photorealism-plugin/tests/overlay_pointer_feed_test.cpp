#include <cstdio>

namespace photorealism {
void log_message(const char*, ...) {}
}

#include "../src/overlay/pointer_feed.cpp"

#include <cassert>
#include <cmath>

using namespace photorealism::overlay;

namespace {

bool close_to(float value, float target) {
    return std::fabs(value - target) < 0.001f;
}

void the_pointer_starts_at_the_middle_of_the_screen() {
    PointerFeed feed;
    feed.reset();
    feed.push(PointerSource::DeviceState, 0, 0, 0, 0);
    const PointerState state = feed.consume(1920.0f, 1080.0f);
    assert(close_to(state.x, 960.0f));
    assert(close_to(state.y, 540.0f));
    assert(state.inside);
}

void deltas_accumulate_instead_of_replacing() {
    PointerFeed feed;
    feed.reset();
    feed.push(PointerSource::DeviceState, 0, 0, 0, 0);
    feed.consume(1920.0f, 1080.0f);

    feed.push(PointerSource::DeviceState, 10, -20, 0, 0);
    feed.push(PointerSource::DeviceState, 5, 4, 0, 0);
    const PointerState state = feed.consume(1920.0f, 1080.0f);
    assert(close_to(state.x, 975.0f));
    assert(close_to(state.y, 524.0f));
}

void the_pointer_never_leaves_the_screen() {
    PointerFeed feed;
    feed.reset();
    feed.push(PointerSource::DeviceState, 0, 0, 0, 0);
    feed.consume(800.0f, 600.0f);

    feed.push(PointerSource::DeviceState, -100000, -100000, 0, 0);
    PointerState state = feed.consume(800.0f, 600.0f);
    assert(close_to(state.x, 0.0f));
    assert(close_to(state.y, 0.0f));

    feed.push(PointerSource::DeviceState, 100000, 100000, 0, 0);
    state = feed.consume(800.0f, 600.0f);
    assert(close_to(state.x, 799.0f));
    assert(close_to(state.y, 599.0f));
}

void the_button_reports_edges_and_not_only_the_level() {
    PointerFeed feed;
    feed.reset();
    feed.push(PointerSource::DeviceState, 0, 0, 0, 0);
    feed.consume(800.0f, 600.0f);

    feed.push(PointerSource::DeviceState, 0, 0, 0, 1);
    PointerState state = feed.consume(800.0f, 600.0f);
    assert(state.down && state.pressed && !state.released);

    state = feed.consume(800.0f, 600.0f);
    assert(state.down && !state.pressed && !state.released);

    feed.push(PointerSource::DeviceState, 0, 0, 0, 0);
    state = feed.consume(800.0f, 600.0f);
    assert(!state.down && !state.pressed && state.released);
}

void the_wheel_comes_back_in_notches() {
    PointerFeed feed;
    feed.reset();
    feed.push(PointerSource::DeviceState, 0, 0, 240, 0);
    const PointerState state = feed.consume(800.0f, 600.0f);
    assert(close_to(state.wheel, 2.0f));
    assert(close_to(feed.consume(800.0f, 600.0f).wheel, 0.0f));
}

void the_first_source_wins_so_movement_is_never_doubled() {
    PointerFeed feed;
    feed.reset();
    feed.push(PointerSource::DeviceState, 0, 0, 0, 0);
    feed.consume(800.0f, 600.0f);

    feed.push(PointerSource::DeviceState, 10, 0, 0, 0);
    feed.push(PointerSource::DeviceData, 10, 0, 0, 0);
    const PointerState state = feed.consume(800.0f, 600.0f);
    assert(close_to(state.x, 410.0f));
}

void a_feed_without_data_is_not_active() {
    PointerFeed feed;
    assert(!feed.active());
    feed.push(PointerSource::DeviceData, 1, 1, 0, 0);
    assert(feed.active());
}

void an_empty_report_never_claims_the_feed() {
    PointerFeed feed;
    feed.reset();
    for (int attempt = 0; attempt < 50; ++attempt) {
        feed.push(PointerSource::DeviceData, 0, 0, 0, 0);
    }
    assert(!feed.active());

    feed.push(PointerSource::DeviceState, 7, 3, 0, 0);
    assert(feed.active());
    feed.consume(800.0f, 600.0f);
    feed.push(PointerSource::DeviceState, 10, 0, 0, 0);
    const PointerState state = feed.consume(800.0f, 600.0f);
    assert(close_to(state.x, 410.0f));
}

void buttons_arrive_even_before_the_first_movement() {
    PointerFeed feed;
    feed.reset();
    feed.push(PointerSource::DeviceData, 0, 0, 0, 1);
    assert(!feed.active());
    feed.push(PointerSource::DeviceState, 1, 0, 0, 1);
    const PointerState state = feed.consume(800.0f, 600.0f);
    assert(state.down);
}

}

int main() {
    the_pointer_starts_at_the_middle_of_the_screen();
    deltas_accumulate_instead_of_replacing();
    the_pointer_never_leaves_the_screen();
    the_button_reports_edges_and_not_only_the_level();
    the_wheel_comes_back_in_notches();
    the_first_source_wins_so_movement_is_never_doubled();
    a_feed_without_data_is_not_active();
    an_empty_report_never_claims_the_feed();
    buttons_arrive_even_before_the_first_movement();
    std::printf("overlay_pointer_feed_test ok\n");
    return 0;
}
