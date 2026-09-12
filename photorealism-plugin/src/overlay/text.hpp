#pragma once

#include "draw_list.hpp"
#include "font.hpp"

namespace photorealism {
namespace overlay {

float text_width(const Font& font, const char* text);

void draw_text(
    DrawList& list,
    const Font& font,
    float x,
    float y,
    const char* text,
    const Color& color);

void draw_text_centered(
    DrawList& list,
    const Font& font,
    const Rect& area,
    const char* text,
    const Color& color);

void draw_text_right(
    DrawList& list,
    const Font& font,
    const Rect& area,
    const char* text,
    const Color& color);

}
}
