#include "overlay.hpp"

#include "theme.hpp"
#include "widgets/controls.hpp"

namespace photorealism {
namespace overlay {
namespace {

void draw_selection(UiContext& ui, const Rect& area) {
    ui.list->push_rect(
        rect_inset(area, -2.0f), theme::kThumb, theme::kControlRadius + 2.0f);
    ui.list->push_rect(area, theme::kPanel, theme::kControlRadius);
}

bool clicked(const UiContext& ui, const Rect& area) {
    return ui.pointer.pressed && rect_contains(area, ui.pointer.x, ui.pointer.y);
}

}

bool Menu::draw_row(
    UiContext& ui,
    const MenuRow& row,
    const Rect& area,
    std::size_t index,
    const Rect& body) {
    if (row.kind == RowKind::Separator) {
        const Rect line = {body.x, area.y, body.width, area.height};
        ui.list->push_rect(line, theme::kSeparator, 0.0f);
        return false;
    }
    selected_ = clicked(ui, area) ? index : selected_;
    if (index == selected_) {
        draw_selection(ui, area);
    }
    if (row.kind == RowKind::Pair) {
        return draw_pair(ui, row, area, body);
    }
    if (row.kind == RowKind::Setting) {
        return draw_setting(ui, row.first, area, body);
    }
    if (button(ui, area, row.first.label, false)) {
        activate_row(row);
    }
    return false;
}

bool Menu::draw_pair(
    UiContext& ui, const MenuRow& row, const Rect& area, const Rect& body) {
    const float half = (area.width - theme::kRowGap) * 0.5f;
    const Rect left = {area.x, area.y, half, area.height};
    const Rect right = {area.x + half + theme::kRowGap, area.y, half, area.height};
    column_ = clicked(ui, left) ? 0 : column_;
    column_ = clicked(ui, right) ? 1 : column_;
    const bool left_shown = draw_setting(ui, row.first, left, body);
    const bool right_shown = draw_setting(ui, row.second, right, body);
    return left_shown || right_shown;
}

bool Menu::draw_setting(
    UiContext& ui,
    const SettingBinding& binding,
    const Rect& area,
    const Rect& body) {
    if (binding.kind == BindingKind::Choice) {
        return draw_choice(ui, binding, area, body);
    }
    if (binding.kind == BindingKind::Toggle) {
        draw_toggle(ui, binding, area);
        return false;
    }
    draw_slider(ui, binding, area);
    return false;
}

void Menu::draw_toggle(
    UiContext& ui, const SettingBinding& binding, const Rect& area) {
    bool value = binding_flag(binding, *settings_);
    if (!toggle_row(ui, area, binding.label, &value)) {
        return;
    }
    set_binding_flag(binding, settings_, value);
    apply_change(binding);
}

void Menu::draw_slider(
    UiContext& ui, const SettingBinding& binding, const Rect& area) {
    float value = binding_value(binding, *settings_);
    const RowResult result = slider_row(ui, area, binding, &value);
    if (result.changed) {
        set_value(binding, value);
    }
    if (result.reset) {
        reset_binding(binding);
    }
}

bool Menu::draw_choice(
    UiContext& ui,
    const SettingBinding& binding,
    const Rect& area,
    const Rect& body) {
    const bool open = open_choice_ == &binding;
    if (choice_row(ui, area, binding, binding_value(binding, *settings_), open)) {
        open_choice_ = open ? nullptr : &binding;
        choice_clicked_ = true;
    }
    if (open_choice_ != &binding) {
        return false;
    }
    open_popup_ = choice_popup_rect(
        choice_box(area, binding), choice_count(binding), body);
    return true;
}

void Menu::draw_open_choice(UiContext& ui, bool shown) {
    if (open_choice_ == nullptr || !shown) {
        open_choice_ = nullptr;
        return;
    }
    const SettingBinding& binding = *open_choice_;
    const int picked =
        choice_popup(ui, open_popup_, binding, binding_value(binding, *settings_));
    if (picked >= 0) {
        open_choice_ = nullptr;
        set_value(binding, binding.minimum + static_cast<float>(picked));
        return;
    }
    if (ui.pointer.pressed && !choice_clicked_) {
        open_choice_ = nullptr;
    }
}

}
}
