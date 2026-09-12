#include "controls.hpp"

#include "../text.hpp"
#include "../theme.hpp"

namespace photorealism {
namespace overlay {
namespace {

bool clicked(const UiContext& ui, const Rect& area) {
    return ui.pointer.pressed && rect_contains(area, ui.pointer.x, ui.pointer.y);
}

Color surface_for(bool hovered, bool accent) {
    if (accent) {
        return hovered ? theme::kAccentHover : theme::kAccent;
    }
    return hovered ? theme::kControlHover : theme::kControl;
}

}

bool button(UiContext& ui, const Rect& area, const char* label, bool accent) {
    const bool hovered = rect_contains(area, ui.pointer.x, ui.pointer.y);
    ui.list->push_rect(
        area, surface_for(hovered, accent), theme::kControlRadius);
    draw_text_centered(*ui.list, *ui.font, area, label, theme::kText);
    return clicked(ui, area);
}

bool toggle_row(UiContext& ui, const Rect& area, const char* label, bool* value) {
    const bool hovered = rect_contains(area, ui.pointer.x, ui.pointer.y);
    ui.list->push_rect(
        area, surface_for(hovered, *value), theme::kControlRadius);

    const Rect text_area = {
        area.x + theme::kPadding * 0.5f,
        area.y,
        area.width - theme::kPadding,
        area.height};
    draw_text(
        *ui.list,
        *ui.font,
        text_area.x,
        text_area.y + (text_area.height - ui.font->line_height()) * 0.5f,
        label,
        theme::kText);
    draw_text_right(
        *ui.list,
        *ui.font,
        text_area,
        *value ? "Ligado" : "Desligado",
        *value ? theme::kText : theme::kTextDim);

    if (!clicked(ui, area)) {
        return false;
    }
    *value = !*value;
    return true;
}

void tab_header(
    UiContext& ui, const Rect& area, const char* label, bool selected) {
    const bool hovered = rect_contains(area, ui.pointer.x, ui.pointer.y);
    const Color surface = selected ? theme::kAccent
                                   : (hovered ? theme::kControlHover
                                              : theme::kControl);
    ui.list->push_rect(area, surface, theme::kControlRadius);
    draw_text_centered(
        *ui.list,
        *ui.font,
        area,
        label,
        selected ? theme::kText : theme::kTextDim);
}

}
}
