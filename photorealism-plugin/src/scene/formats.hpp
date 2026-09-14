#pragma once

namespace photorealism {
namespace scene_formats {
constexpr unsigned kR8G8B8A8Typeless = 0x1bu;
constexpr unsigned kR8G8B8A8Unorm = 0x1cu;
constexpr unsigned kR8G8B8A8UnormSrgb = 0x1du;
constexpr unsigned kB8G8R8A8Unorm = 0x57u;
constexpr unsigned kB8G8R8X8Unorm = 0x58u;
constexpr unsigned kB8G8R8A8Typeless = 0x5au;
constexpr unsigned kB8G8R8A8UnormSrgb = 0x5bu;
constexpr unsigned kB8G8R8X8Typeless = 0x5cu;
constexpr unsigned kB8G8R8X8UnormSrgb = 0x5du;

inline bool is_bgra(unsigned format) {
    return format == kB8G8R8A8Unorm || format == kB8G8R8A8UnormSrgb ||
           format == kB8G8R8A8Typeless || format == kB8G8R8X8Unorm ||
           format == kB8G8R8X8UnormSrgb || format == kB8G8R8X8Typeless;
}

inline bool is_rgba(unsigned format) {
    return format == kR8G8B8A8Unorm || format == kR8G8B8A8UnormSrgb ||
           format == kR8G8B8A8Typeless;
}

inline bool is_readable(unsigned format) {
    return is_bgra(format) || is_rgba(format);
}

inline bool is_srgb(unsigned format) {
    return format == kR8G8B8A8UnormSrgb || format == kB8G8R8A8UnormSrgb ||
           format == kB8G8R8X8UnormSrgb;
}

inline unsigned resolve_typeless(unsigned format) {
    if (format == kB8G8R8X8Unorm || format == kB8G8R8X8UnormSrgb ||
        format == kB8G8R8X8Typeless) {
        return kB8G8R8X8Typeless;
    }
    if (is_bgra(format)) {
        return kB8G8R8A8Typeless;
    }
    return kR8G8B8A8Typeless;
}

inline unsigned resolve_unorm(unsigned format) {
    if (format == kB8G8R8X8Unorm || format == kB8G8R8X8UnormSrgb ||
        format == kB8G8R8X8Typeless) {
        return kB8G8R8X8Unorm;
    }
    if (is_bgra(format)) {
        return kB8G8R8A8Unorm;
    }
    return kR8G8B8A8Unorm;
}
}
}
