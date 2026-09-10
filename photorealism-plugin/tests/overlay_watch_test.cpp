#include "../src/overlay_watch.hpp"

#include <cassert>

using namespace photorealism;

namespace {

const char kOverlayA = 'A';
const char kOverlayB = 'B';
const void* const kHandleA = &kOverlayA;
const void* const kHandleB = &kOverlayB;

constexpr unsigned kRequired = 4;

OverlayWatch feed(OverlayWatch watch, const void* handle, unsigned times) {
    for (unsigned i = 0; i < times; ++i) {
        watch = advance_overlay_watch(watch, handle);
    }
    return watch;
}

unsigned steps_until_stable(const void* handle) {
    OverlayWatch watch = {};
    for (unsigned step = 1; step <= 64u; ++step) {
        watch = advance_overlay_watch(watch, handle);
        if (overlay_is_stable(watch, kRequired)) {
            return step;
        }
    }
    return 0u;
}

}  // namespace

int main() {
    {
        OverlayWatch watch = {};
        watch = feed(watch, nullptr, 30);
        assert(watch.module == nullptr);
        assert(watch.stable_samples == 0u);
        assert(!overlay_is_stable(watch, kRequired));
    }

    {
        assert(steps_until_stable(kHandleA) == kRequired);
    }

    {
        OverlayWatch watch = {};
        watch = advance_overlay_watch(watch, kHandleA);
        assert(watch.module == kHandleA);
        assert(watch.stable_samples == 1u);
        assert(!overlay_is_stable(watch, kRequired));
    }

    {
        OverlayWatch watch = {};
        watch = feed(watch, kHandleA, 3);
        assert(watch.stable_samples == 3u);
        assert(!overlay_is_stable(watch, kRequired));

        watch = advance_overlay_watch(watch, kHandleB);
        assert(watch.module == kHandleB);
        assert(watch.stable_samples == 1u);
        assert(!overlay_is_stable(watch, kRequired));

        watch = feed(watch, kHandleB, 3);
        assert(watch.stable_samples == 4u);
        assert(overlay_is_stable(watch, kRequired));
    }

    {
        OverlayWatch watch = {};
        watch = feed(watch, kHandleA, 3);
        watch = advance_overlay_watch(watch, nullptr);
        assert(watch.module == nullptr);
        assert(watch.stable_samples == 0u);
        assert(!overlay_is_stable(watch, kRequired));

        watch = feed(watch, kHandleA, 3);
        assert(watch.stable_samples == 3u);
        assert(!overlay_is_stable(watch, kRequired));
        watch = advance_overlay_watch(watch, kHandleA);
        assert(overlay_is_stable(watch, kRequired));
    }

    {
        OverlayWatch watch = {};
        for (unsigned i = 0; i < 10u; ++i) {
            watch = advance_overlay_watch(watch, i % 2u == 0u ? kHandleA : kHandleB);
            assert(watch.stable_samples == 1u);
            assert(!overlay_is_stable(watch, kRequired));
        }
    }

    {
        OverlayWatch watch = {};
        watch = feed(watch, kHandleA, 8);
        assert(overlay_is_stable(watch, kRequired));
        assert(!overlay_is_stable(watch, 9u));
        assert(overlay_is_stable(watch, 1u));
    }

    {
        OverlayWatch watch = {};
        watch.module = nullptr;
        watch.stable_samples = 99u;
        assert(!overlay_is_stable(watch, kRequired));
    }

    return 0;
}
