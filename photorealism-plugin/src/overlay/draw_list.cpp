#include "draw_list.hpp"

namespace photorealism {
namespace overlay {
namespace {

float remap(float value, float from_start, float from_size, float to_start, float to_size) {
    if (from_size <= 0.0f) {
        return to_start;
    }
    return to_start + (value - from_start) / from_size * to_size;
}

Rect uv_for_visible(const Rect& rect, const Rect& uv, const Rect& visible) {
    Rect result = {};
    result.x = remap(visible.x, rect.x, rect.width, uv.x, uv.width);
    result.y = remap(visible.y, rect.y, rect.height, uv.y, uv.height);
    const float right =
        remap(visible.x + visible.width, rect.x, rect.width, uv.x, uv.width);
    const float bottom =
        remap(visible.y + visible.height, rect.y, rect.height, uv.y, uv.height);
    result.width = right - result.x;
    result.height = bottom - result.y;
    return result;
}

}

bool rect_contains(const Rect& rect, float x, float y) {
    return x >= rect.x && x < rect.x + rect.width && y >= rect.y &&
           y < rect.y + rect.height;
}

bool rect_is_empty(const Rect& rect) {
    return rect.width <= 0.0f || rect.height <= 0.0f;
}

Rect rect_intersection(const Rect& first, const Rect& second) {
    const float left = first.x > second.x ? first.x : second.x;
    const float top = first.y > second.y ? first.y : second.y;
    const float first_right = first.x + first.width;
    const float second_right = second.x + second.width;
    const float first_bottom = first.y + first.height;
    const float second_bottom = second.y + second.height;
    const float right = first_right < second_right ? first_right : second_right;
    const float bottom =
        first_bottom < second_bottom ? first_bottom : second_bottom;

    Rect result = {};
    result.x = left;
    result.y = top;
    result.width = right - left;
    result.height = bottom - top;
    return result;
}

Rect rect_inset(const Rect& rect, float amount) {
    Rect result = {};
    result.x = rect.x + amount;
    result.y = rect.y + amount;
    result.width = rect.width - amount * 2.0f;
    result.height = rect.height - amount * 2.0f;
    return result;
}

void DrawList::begin(float width, float height) {
    vertices_.clear();
    clips_.clear();
    width_ = width > 0.0f ? width : 1.0f;
    height_ = height > 0.0f ? height : 1.0f;
}

Rect DrawList::clip() const {
    if (clips_.empty()) {
        const Rect full = {0.0f, 0.0f, width_, height_};
        return full;
    }
    return clips_.back();
}

void DrawList::push_clip(const Rect& rect) {
    clips_.push_back(rect_intersection(clip(), rect));
}

void DrawList::pop_clip() {
    if (clips_.empty()) {
        return;
    }
    clips_.pop_back();
}

void DrawList::push_rect(const Rect& rect, const Color& color, float radius) {
    const Rect uv = {0.0f, 0.0f, 0.0f, 0.0f};
    push_quad(rect, uv, color, radius, kKindSolid);
}

void DrawList::push_glyph(const Rect& rect, const Rect& uv, const Color& color) {
    push_quad(rect, uv, color, 0.0f, kKindGlyph);
}

void DrawList::push_quad(
    const Rect& rect,
    const Rect& uv,
    const Color& color,
    float radius,
    float kind) {
    if (rect_is_empty(rect)) {
        return;
    }
    const Rect visible = rect_intersection(rect, clip());
    if (rect_is_empty(visible)) {
        return;
    }

    const Rect visible_uv = uv_for_visible(rect, uv, visible);
    const float center_x = rect.x + rect.width * 0.5f;
    const float center_y = rect.y + rect.height * 0.5f;
    const float half_x = rect.width * 0.5f;
    const float half_y = rect.height * 0.5f;
    const float corners_x[2] = {visible.x, visible.x + visible.width};
    const float corners_y[2] = {visible.y, visible.y + visible.height};
    const float corners_u[2] = {visible_uv.x, visible_uv.x + visible_uv.width};
    const float corners_v[2] = {visible_uv.y, visible_uv.y + visible_uv.height};
    const int order[kVerticesPerQuad][2] = {
        {0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}};

    for (const int(&corner)[2] : order) {
        const float x = corners_x[corner[0]];
        const float y = corners_y[corner[1]];
        Vertex vertex = {};
        vertex.position[0] = x / width_ * 2.0f - 1.0f;
        vertex.position[1] = 1.0f - y / height_ * 2.0f;
        vertex.uv[0] = corners_u[corner[0]];
        vertex.uv[1] = corners_v[corner[1]];
        vertex.color[0] = color.r;
        vertex.color[1] = color.g;
        vertex.color[2] = color.b;
        vertex.color[3] = color.a;
        vertex.local[0] = x - center_x;
        vertex.local[1] = y - center_y;
        vertex.half_extent[0] = half_x;
        vertex.half_extent[1] = half_y;
        vertex.radius = radius;
        vertex.kind = kind;
        vertices_.push_back(vertex);
    }
}

}
}
