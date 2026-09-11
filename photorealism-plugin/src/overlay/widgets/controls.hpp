#pragma once

#include "../bindings/setting_binding.hpp"
#include "../draw_list.hpp"
#include "../font.hpp"
#include "../input_state.hpp"

namespace photorealism {
namespace overlay {

struct UiContext {
    DrawList* list = nullptr;
    const Font* font = nullptr;
    PointerState pointer;
    const void* active = nullptr;
};

struct RowResult {
    bool changed = false;
    bool reset = false;
};

bool button(
    UiContext& ui, const Rect& area, const char* label, bool accent);

bool toggle_row(
    UiContext& ui, const Rect& area, const char* label, bool* value);

RowResult slider_row(
    UiContext& ui,
    const Rect& area,
    const SettingBinding& binding,
    float* value);

void tab_header(
    UiContext& ui, const Rect& area, const char* label, bool selected);

}
}
