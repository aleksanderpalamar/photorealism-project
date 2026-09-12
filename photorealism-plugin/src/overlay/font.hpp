#pragma once

#include <vector>

namespace photorealism {
namespace overlay {

constexpr unsigned kFirstGlyph = 32;
constexpr unsigned kGlyphCount = 95;

struct Glyph {
    float u0;
    float v0;
    float u1;
    float v1;
    float offset_x;
    float offset_y;
    float width;
    float height;
    float advance;
};

struct BakedFont {
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    Glyph glyphs[kGlyphCount] = {};
    float line_height = 0.0f;
    float ascent = 0.0f;
    bool smooth = false;
};

class Font {
  public:
    virtual ~Font() = default;
    virtual const Glyph* glyph(unsigned code) const = 0;
    virtual float line_height() const = 0;
};

class BakedFontView : public Font {
  public:
    BakedFontView() = default;
    explicit BakedFontView(const BakedFont* font) : font_(font) {}

    void reset(const BakedFont* font) { font_ = font; }
    bool valid() const { return font_ != nullptr; }

    const Glyph* glyph(unsigned code) const override;
    float line_height() const override;

  private:
    const BakedFont* font_ = nullptr;
};

}
}
