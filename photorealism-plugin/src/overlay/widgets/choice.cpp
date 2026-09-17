#include "controls.hpp"

#include "../layout.hpp"
#include "../text.hpp"
#include "../theme.hpp"

namespace photorealism {
namespace overlay {
namespace {

constexpr float kLabelFraction = 0.40f;
constexpr float kPopupGap = 2.0f;
constexpr int kArrowRows = 6;

void draw_arrow(DrawList& list, const Rect& box) {
    const float center = box.x + box.width - theme::kPadding;
    const float top = box.y + (box.height - static_cast<float>(kArrowRows)) * 0.5f;
    for (int row = 0; row < kArrowRows; ++row) {
        const float half = static_cast<float>(kArrowRows - row);
        const Rect line = {
            center - half, top + static_cast<float>(row), half * 2.0f, 1.0f};
        list.push_rect(line, theme::kText, 0.0f);
    }
}

float text_top(const UiContext& ui, const Rect& area) {
    return area.y + (area.height - ui.font->line_height()) * 0.5f;
}

const char* choice_label(const SettingBinding& binding, float value) {
    return binding.choices[choice_index(binding, value)];
}

}

Rect choice_box(const Rect& area, const SettingBinding& binding) {
    if (binding.label == nullptr || binding.label[0] == '\0') {
        return area;
    }
    const Rect label_area = take_left(area, area.width * kLabelFraction);
    const float start = label_area.x + label_area.width + theme::kRowGap;
    return {start, area.y, area.x + area.width - start, area.height};
}

bool choice_row(
    UiContext& ui,
    const Rect& area,
    const SettingBinding& binding,
    float value,
    bool open) {
    const Rect box = choice_box(area, binding);
    if (box.x > area.x) {
        draw_text(
            *ui.list, *ui.font, area.x, text_top(ui, area), binding.label,
            theme::kText);
    }
    const bool hovered = rect_contains(box, ui.pointer.x, ui.pointer.y);
    ui.list->push_rect(
        box,
        open || hovered ? theme::kControlHover : theme::kControl,
        theme::kControlRadius);
    draw_text(
        *ui.list, *ui.font, box.x + theme::kPadding * 0.5f, text_top(ui, box),
        choice_label(binding, value), theme::kText);
    draw_arrow(*ui.list, box);
    return ui.pointer.pressed && hovered;
}

Rect choice_popup_rect(
    const Rect& box, std::size_t count, const Rect& bounds) {
    const float height = static_cast<float>(count) * theme::kRowHeight;
    const float below = box.y + box.height + kPopupGap;
    const bool fits_below = below + height <= bounds.y + bounds.height;
    const float top = fits_below ? below : box.y - kPopupGap - height;
    return {box.x, top, box.width, height};
}

int choice_popup(
    UiContext& ui,
    const Rect& popup,
    const SettingBinding& binding,
    float value) {
    ui.list->push_rect(
        rect_inset(popup, -theme::kBorderWidth), theme::kPanelBorder,
        theme::kControlRadius + theme::kBorderWidth);
    ui.list->push_rect(popup, theme::kPanel, theme::kControlRadius);

    const std::size_t current = choice_index(binding, value);
    int picked = -1;
    for (std::size_t index = 0; index < choice_count(binding); ++index) {
        const Rect option = {
            popup.x,
            popup.y + static_cast<float>(index) * theme::kRowHeight,
            popup.width,
            theme::kRowHeight};
        const bool selectable = choice_selectable(binding, index);
        const bool hovered =
            selectable && rect_contains(option, ui.pointer.x, ui.pointer.y);
        const bool selected = index == current;
        if (hovered || selected) {
            ui.list->push_rect(
                option, selected ? theme::kAccent : theme::kControlHover,
                theme::kControlRadius);
        }
        draw_text(
            *ui.list, *ui.font, option.x + theme::kPadding * 0.5f,
            text_top(ui, option), binding.choices[index],
            selectable ? theme::kText : theme::kTextDim);
        picked = ui.pointer.pressed && hovered ? static_cast<int>(index) : picked;
    }
    return picked;
}

}
}
