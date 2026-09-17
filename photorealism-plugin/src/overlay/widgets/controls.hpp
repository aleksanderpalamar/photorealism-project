#pragma once

#include "../bindings/setting_binding.hpp"
#include "../draw_list.hpp"
#include "../font.hpp"
#include "../input_state.hpp"

#include <cstddef>

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

bool choice_row(
    UiContext& ui,
    const Rect& area,
    const SettingBinding& binding,
    float value,
    bool open);

Rect choice_box(const Rect& area, const SettingBinding& binding);

Rect choice_popup_rect(
    const Rect& box, std::size_t count, const Rect& bounds);

int choice_popup(
    UiContext& ui,
    const Rect& popup,
    const SettingBinding& binding,
    float value);

}
}
