#include "text.hpp"

namespace photorealism {
namespace overlay {
namespace {

const Glyph* resolve(const Font& font, unsigned char code) {
    const Glyph* glyph = font.glyph(code);
    if (glyph != nullptr) {
        return glyph;
    }
    return font.glyph(static_cast<unsigned>('?'));
}

float baseline_offset(const Font& font, const Rect& area) {
    return area.y + (area.height - font.line_height()) * 0.5f;
}

}

float text_width(const Font& font, const char* text) {
    if (text == nullptr) {
        return 0.0f;
    }
    float total = 0.0f;
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        const Glyph* glyph = resolve(font, static_cast<unsigned char>(*cursor));
        if (glyph == nullptr) {
            continue;
        }
        total += glyph->advance;
    }
    return total;
}

void draw_text(
    DrawList& list,
    const Font& font,
    float x,
    float y,
    const char* text,
    const Color& color) {
    if (text == nullptr) {
        return;
    }
    float pen = x;
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        const Glyph* glyph = resolve(font, static_cast<unsigned char>(*cursor));
        if (glyph == nullptr) {
            continue;
        }
        const Rect target = {
            pen + glyph->offset_x,
            y + glyph->offset_y,
            glyph->width,
            glyph->height};
        const Rect uv = {
            glyph->u0,
            glyph->v0,
            glyph->u1 - glyph->u0,
            glyph->v1 - glyph->v0};
        list.push_glyph(target, uv, color);
        pen += glyph->advance;
    }
}

void draw_text_centered(
    DrawList& list,
    const Font& font,
    const Rect& area,
    const char* text,
    const Color& color) {
    const float width = text_width(font, text);
    const float x = area.x + (area.width - width) * 0.5f;
    draw_text(list, font, x, baseline_offset(font, area), text, color);
}

void draw_text_right(
    DrawList& list,
    const Font& font,
    const Rect& area,
    const char* text,
    const Color& color) {
    const float width = text_width(font, text);
    const float x = area.x + area.width - width;
    draw_text(list, font, x, baseline_offset(font, area), text, color);
}

}
}
