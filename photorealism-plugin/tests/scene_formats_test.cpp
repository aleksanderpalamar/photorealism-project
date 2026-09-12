#include "../src/scene/formats.hpp"

#include <cassert>

using namespace photorealism::scene_formats;

int main() {
    assert(is_readable(kB8G8R8A8Typeless));
    assert(is_bgra(kB8G8R8A8Typeless));
    assert(resolve_unorm(kB8G8R8A8Typeless) == kB8G8R8A8Unorm);

    assert(resolve_unorm(kB8G8R8A8Unorm) == resolve_unorm(kB8G8R8A8Typeless));

    assert(resolve_typeless(kR8G8B8A8Unorm) == kR8G8B8A8Typeless);
    assert(resolve_typeless(kR8G8B8A8UnormSrgb) == kR8G8B8A8Typeless);
    assert(resolve_typeless(kR8G8B8A8Typeless) == kR8G8B8A8Typeless);
    assert(resolve_typeless(kB8G8R8A8Unorm) == kB8G8R8A8Typeless);
    assert(resolve_typeless(kB8G8R8A8UnormSrgb) == kB8G8R8A8Typeless);
    assert(resolve_typeless(kB8G8R8X8Unorm) == kB8G8R8X8Typeless);

    const unsigned bgra[] = {
        kB8G8R8A8Unorm, kB8G8R8A8UnormSrgb, kB8G8R8A8Typeless};
    for (unsigned format : bgra) {
        assert(is_readable(format));
        assert(is_bgra(format));
        assert(!is_rgba(format));
        assert(resolve_unorm(format) == kB8G8R8A8Unorm);
    }
    const unsigned bgrx[] = {
        kB8G8R8X8Unorm, kB8G8R8X8UnormSrgb, kB8G8R8X8Typeless};
    for (unsigned format : bgrx) {
        assert(is_readable(format));
        assert(is_bgra(format));
        assert(resolve_unorm(format) == kB8G8R8X8Unorm);
    }
    const unsigned rgba[] = {
        kR8G8B8A8Unorm, kR8G8B8A8UnormSrgb, kR8G8B8A8Typeless};
    for (unsigned format : rgba) {
        assert(is_readable(format));
        assert(is_rgba(format));
        assert(!is_bgra(format));
        assert(resolve_unorm(format) == kR8G8B8A8Unorm);
    }

    const unsigned every[] = {
        kR8G8B8A8Typeless, kR8G8B8A8Unorm, kR8G8B8A8UnormSrgb,
        kB8G8R8A8Unorm, kB8G8R8X8Unorm, kB8G8R8A8Typeless,
        kB8G8R8A8UnormSrgb, kB8G8R8X8Typeless, kB8G8R8X8UnormSrgb};
    for (unsigned format : every) {
        const unsigned resolved = resolve_unorm(format);
        assert(resolved != kR8G8B8A8UnormSrgb);
        assert(resolved != kB8G8R8A8UnormSrgb);
        assert(resolved != kB8G8R8X8UnormSrgb);
        assert(resolved != kR8G8B8A8Typeless);
        assert(resolved != kB8G8R8A8Typeless);
        assert(resolved != kB8G8R8X8Typeless);
        assert(is_readable(resolved));

        assert(resolve_unorm(resolved) == resolved);

        assert(is_bgra(format) == is_bgra(resolved));
    }

    const unsigned unreadable[] = {0u, 2u, 10u, 24u, 40u, 71u, 95u};
    for (unsigned format : unreadable) {
        assert(!is_readable(format));
    }

    return 0;
}
