#pragma once

#include <cstddef>
#include <vector>

namespace photorealism {
namespace overlay {

struct Color {
    float r;
    float g;
    float b;
    float a;
};

struct Rect {
    float x;
    float y;
    float width;
    float height;
};

struct Vertex {
    float position[2];
    float uv[2];
    float color[4];
    float local[2];
    float half_extent[2];
    float radius;
    float kind;
    float padding[2];
};

static_assert(sizeof(Vertex) == 64);

constexpr float kKindSolid = 0.0f;
constexpr float kKindGlyph = 1.0f;
constexpr std::size_t kVerticesPerQuad = 6;

bool rect_contains(const Rect& rect, float x, float y);
Rect rect_intersection(const Rect& first, const Rect& second);
bool rect_is_empty(const Rect& rect);
Rect rect_inset(const Rect& rect, float amount);

class DrawList {
  public:
    void begin(float width, float height);
    void push_rect(const Rect& rect, const Color& color, float radius);
    void push_glyph(const Rect& rect, const Rect& uv, const Color& color);
    void push_clip(const Rect& rect);
    void pop_clip();

    const std::vector<Vertex>& vertices() const { return vertices_; }
    std::size_t vertex_count() const { return vertices_.size(); }
    float width() const { return width_; }
    float height() const { return height_; }
    Rect clip() const;

  private:
    void push_quad(
        const Rect& rect,
        const Rect& uv,
        const Color& color,
        float radius,
        float kind);

    std::vector<Vertex> vertices_;
    std::vector<Rect> clips_;
    float width_ = 1.0f;
    float height_ = 1.0f;
};

}
}
