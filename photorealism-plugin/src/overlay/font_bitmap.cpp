#include "font_bitmap.hpp"

#include "font_embedded.hpp"

#include <cstddef>

namespace photorealism {
namespace overlay {
namespace {

int atlas_rows() {
    return static_cast<int>(
        (kGlyphCount + kEmbeddedColumns - 1) / kEmbeddedColumns);
}

void stamp_glyph(BakedFont* font, unsigned index, int cell_x, int cell_y) {
    for (int column = 0; column < kEmbeddedGlyphWidth; ++column) {
        const unsigned char bits = kEmbeddedGlyphs[index][column];
        for (int row = 0; row < kEmbeddedGlyphHeight; ++row) {
            if (((bits >> row) & 1) == 0) {
                continue;
            }
            const int x = cell_x + column;
            const int y = cell_y + row;
            font->pixels[static_cast<std::size_t>(y) * font->width + x] = 255;
        }
    }
}

void fill_metrics(
    BakedFont* font, unsigned index, int cell_x, int cell_y, float scale) {
    const float width = static_cast<float>(font->width);
    const float height = static_cast<float>(font->height);
    Glyph& glyph = font->glyphs[index];
    glyph.u0 = static_cast<float>(cell_x) / width;
    glyph.v0 = static_cast<float>(cell_y) / height;
    glyph.u1 = static_cast<float>(cell_x + kEmbeddedGlyphWidth) / width;
    glyph.v1 = static_cast<float>(cell_y + kEmbeddedGlyphHeight) / height;
    glyph.offset_x = 0.0f;
    glyph.offset_y = 0.0f;
    glyph.width = kEmbeddedGlyphWidth * scale;
    glyph.height = kEmbeddedGlyphHeight * scale;
    glyph.advance = kEmbeddedCellWidth * scale;
}

}

BakedFont bake_embedded_font(int scale) {
    const int safe_scale = scale > 0 ? scale : 1;
    const float scale_value = static_cast<float>(safe_scale);

    BakedFont font;
    font.width = kEmbeddedColumns * kEmbeddedCellWidth;
    font.height = atlas_rows() * kEmbeddedCellHeight;
    font.pixels.assign(
        static_cast<std::size_t>(font.width) * font.height, 0);
    font.line_height = kEmbeddedCellHeight * scale_value;
    font.ascent = kEmbeddedGlyphHeight * scale_value;

    for (unsigned index = 0; index < kGlyphCount; ++index) {
        const int cell_x =
            static_cast<int>(index % kEmbeddedColumns) * kEmbeddedCellWidth;
        const int cell_y =
            static_cast<int>(index / kEmbeddedColumns) * kEmbeddedCellHeight;
        stamp_glyph(&font, index, cell_x, cell_y);
        fill_metrics(&font, index, cell_x, cell_y, scale_value);
    }
    return font;
}

}
}
