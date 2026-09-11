#include "../src/overlay/draw_list.cpp"
#include "../src/overlay/font.cpp"
#include "../src/overlay/font_bitmap.cpp"
#include "../src/overlay/text.cpp"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace photorealism::overlay;

namespace {

bool close_to(float value, float target) {
    return std::fabs(value - target) < 0.0001f;
}

void quad_emits_six_vertices() {
    DrawList list;
    list.begin(800.0f, 600.0f);
    const Rect rect = {10.0f, 20.0f, 100.0f, 40.0f};
    const Color color = {1.0f, 0.5f, 0.25f, 1.0f};
    list.push_rect(rect, color, 4.0f);
    assert(list.vertex_count() == kVerticesPerQuad);
}

void full_screen_quad_spans_normalized_device() {
    DrawList list;
    list.begin(800.0f, 600.0f);
    const Rect rect = {0.0f, 0.0f, 800.0f, 600.0f};
    const Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    list.push_rect(rect, color, 0.0f);

    const std::vector<Vertex>& vertices = list.vertices();
    assert(close_to(vertices[0].position[0], -1.0f));
    assert(close_to(vertices[0].position[1], 1.0f));
    assert(close_to(vertices[2].position[0], 1.0f));
    assert(close_to(vertices[2].position[1], -1.0f));
}

void clip_outside_drops_the_quad() {
    DrawList list;
    list.begin(800.0f, 600.0f);
    const Rect clip = {0.0f, 0.0f, 100.0f, 100.0f};
    const Rect rect = {400.0f, 400.0f, 50.0f, 50.0f};
    const Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    list.push_clip(clip);
    list.push_rect(rect, color, 0.0f);
    list.pop_clip();
    assert(list.vertex_count() == 0);
}

void partial_clip_keeps_the_original_shape_reference() {
    DrawList list;
    list.begin(800.0f, 600.0f);
    const Rect rect = {100.0f, 100.0f, 200.0f, 100.0f};
    const Rect clip = {0.0f, 0.0f, 200.0f, 600.0f};
    const Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    list.push_clip(clip);
    list.push_rect(rect, color, 8.0f);
    list.pop_clip();

    assert(list.vertex_count() == kVerticesPerQuad);
    const Vertex& first = list.vertices()[0];
    assert(close_to(first.half_extent[0], 100.0f));
    assert(close_to(first.half_extent[1], 50.0f));
    assert(close_to(first.local[0], -100.0f));
    assert(close_to(first.radius, 8.0f));

    const Vertex& right = list.vertices()[1];
    const float clipped_x = right.position[0] * 0.5f * 800.0f + 400.0f;
    assert(close_to(clipped_x, 200.0f));
    assert(close_to(right.local[0], 0.0f));
}

void clipped_glyph_keeps_its_atlas_proportion() {
    DrawList list;
    list.begin(800.0f, 600.0f);
    const Rect rect = {100.0f, 100.0f, 20.0f, 20.0f};
    const Rect uv = {0.0f, 0.0f, 0.5f, 0.5f};
    const Rect clip = {0.0f, 0.0f, 110.0f, 600.0f};
    const Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    list.push_clip(clip);
    list.push_glyph(rect, uv, color);
    list.pop_clip();

    assert(list.vertex_count() == kVerticesPerQuad);
    assert(close_to(list.vertices()[0].uv[0], 0.0f));
    assert(close_to(list.vertices()[1].uv[0], 0.25f));
    assert(close_to(list.vertices()[0].kind, kKindGlyph));
}

void embedded_atlas_has_every_glyph() {
    const BakedFont font = bake_embedded_font(2);
    assert(font.width == 96);
    assert(font.height == 48);
    assert(font.pixels.size() == static_cast<std::size_t>(96 * 48));
    assert(close_to(font.line_height, 16.0f));

    BakedFontView view(&font);
    assert(view.glyph(static_cast<unsigned>(' ')) != nullptr);
    assert(view.glyph(static_cast<unsigned>('~')) != nullptr);
    assert(view.glyph(31) == nullptr);
    assert(view.glyph(127) == nullptr);
    assert(close_to(view.glyph(static_cast<unsigned>('A'))->advance, 12.0f));
}

void space_is_blank_and_letters_are_not() {
    const BakedFont font = bake_embedded_font(1);
    std::size_t space_pixels = 0;
    std::size_t letter_pixels = 0;
    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
            const std::size_t space_index =
                static_cast<std::size_t>(row) * font.width + column;
            const int letter_cell_x = (static_cast<int>('A') - 32) % 16 * 6;
            const int letter_cell_y = (static_cast<int>('A') - 32) / 16 * 8;
            const std::size_t letter_index =
                static_cast<std::size_t>(letter_cell_y + row) * font.width +
                letter_cell_x + column;
            space_pixels += font.pixels[space_index] != 0 ? 1 : 0;
            letter_pixels += font.pixels[letter_index] != 0 ? 1 : 0;
        }
    }
    assert(space_pixels == 0);
    assert(letter_pixels > 0);
}

void text_width_is_the_sum_of_advances() {
    const BakedFont font = bake_embedded_font(2);
    BakedFontView view(&font);
    assert(close_to(text_width(view, ""), 0.0f));
    assert(close_to(text_width(view, "SSAO"), 48.0f));
    assert(close_to(text_width(view, nullptr), 0.0f));
}

void drawn_text_emits_one_quad_per_visible_glyph() {
    const BakedFont font = bake_embedded_font(2);
    BakedFontView view(&font);
    DrawList list;
    list.begin(800.0f, 600.0f);
    const Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    draw_text(list, view, 10.0f, 10.0f, "Clima", color);
    assert(list.vertex_count() == 5 * kVerticesPerQuad);
}

void centered_text_sits_in_the_middle() {
    const BakedFont font = bake_embedded_font(2);
    BakedFontView view(&font);
    DrawList list;
    list.begin(800.0f, 600.0f);
    const Rect area = {0.0f, 0.0f, 200.0f, 40.0f};
    const Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    draw_text_centered(list, view, area, "AB", color);

    const float expected_x = (200.0f - 24.0f) * 0.5f;
    const float actual_x =
        (list.vertices()[0].position[0] + 1.0f) * 0.5f * 800.0f;
    assert(close_to(actual_x, expected_x));
}

}

int main() {
    quad_emits_six_vertices();
    full_screen_quad_spans_normalized_device();
    clip_outside_drops_the_quad();
    partial_clip_keeps_the_original_shape_reference();
    clipped_glyph_keeps_its_atlas_proportion();
    embedded_atlas_has_every_glyph();
    space_is_blank_and_letters_are_not();
    text_width_is_the_sum_of_advances();
    drawn_text_emits_one_quad_per_visible_glyph();
    centered_text_sits_in_the_middle();
    std::printf("overlay_draw_list_test ok\n");
    return 0;
}
