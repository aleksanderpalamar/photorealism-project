#include "controls.hpp"

#include "../text.hpp"
#include "../theme.hpp"

#include <cstdio>

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
    char text[96] = {};
    std::snprintf(
        text, sizeof(text), "%s: %s", label, *value ? "ligado" : "desligado");
    draw_text_centered(*ui.list, *ui.font, area, text, theme::kText);
    if (!clicked(ui, area)) {
        return false;
    }
    *value = !*value;
    return true;
}

}
}
