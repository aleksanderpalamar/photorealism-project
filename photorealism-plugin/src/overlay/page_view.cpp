#include "layout.hpp"

#include <cstring>
#include "overlay.hpp"
#include "text.hpp"
#include "theme.hpp"
#include "widgets/controls.hpp"

namespace photorealism {
namespace overlay {
namespace {

constexpr float kScrollStep = 48.0f;
constexpr float kScrollBarWidth = 4.0f;
constexpr float kKeyboardSteps = 100.0f;

std::size_t move_index(std::size_t current, std::size_t count, int delta) {
    if (count == 0) {
        return 0;
    }
    const long long total = static_cast<long long>(count);
    long long moved = static_cast<long long>(current) + delta;
    while (moved < 0) {
        moved += total;
    }
    return static_cast<std::size_t>(moved % total);
}

float row_top(std::size_t index) {
    return static_cast<float>(index) * (theme::kRowHeight + theme::kRowGap);
}

float content_height_of(const SettingPage& page) {
    return static_cast<float>(page.count) *
           (theme::kRowHeight + theme::kRowGap);
}

float clamp_scroll(float scroll, float content, float view) {
    const float limit = content - view;
    if (limit <= 0.0f) {
        return 0.0f;
    }
    if (scroll < 0.0f) {
        return 0.0f;
    }
    return scroll > limit ? limit : scroll;
}

void draw_scroll_bar(
    DrawList& list, const Rect& body, float scroll, float content) {
    if (content <= body.height) {
        return;
    }
    const Rect track = {
        body.x + body.width - kScrollBarWidth,
        body.y,
        kScrollBarWidth,
        body.height};
    list.push_rect(track, theme::kTrack, kScrollBarWidth * 0.5f);

    const float ratio = body.height / content;
    const float thumb_height = body.height * ratio;
    const float travel = body.height - thumb_height;
    const float offset = travel * (scroll / (content - body.height));
    const Rect thumb = {
        track.x, body.y + offset, kScrollBarWidth, thumb_height};
    list.push_rect(thumb, theme::kThumb, kScrollBarWidth * 0.5f);
}

}

void Menu::step_selection(const SettingPage& page, const Rect& body) {
    if ((keys_ & kKeyDown) != 0) {
        selected_ = move_index(selected_, page.count, 1);
    }
    if ((keys_ & kKeyUp) != 0) {
        selected_ = move_index(selected_, page.count, -1);
    }
    if (selected_ >= page.count) {
        selected_ = 0;
    }

    const float top = row_top(selected_);
    const float bottom = top + theme::kRowHeight;
    if (top < scroll_) {
        scroll_ = top;
    }
    if (bottom > scroll_ + body.height) {
        scroll_ = bottom - body.height;
    }

    const SettingBinding& binding = page.items[selected_];
    const bool wants_toggle = (keys_ & kKeyEnter) != 0;
    if (binding.kind == BindingKind::Toggle) {
        if (!wants_toggle) {
            return;
        }
        set_binding_flag(binding, settings_, !binding_flag(binding, *settings_));
        apply_change(binding);
        return;
    }
    if (wants_toggle) {
        reset_binding(binding);
        return;
    }
    if (binding.inert) {
        return;
    }

    const int direction = ((keys_ & kKeyRight) != 0 ? 1 : 0) -
                          ((keys_ & kKeyLeft) != 0 ? 1 : 0);
    if (direction == 0) {
        return;
    }
    const float step = (binding.maximum - binding.minimum) / kKeyboardSteps;
    const float updated = binding_value(binding, *settings_) +
                          step * static_cast<float>(direction);
    set_binding_value(binding, settings_, updated);
    apply_change(binding);
}

void Menu::draw_body(UiContext& ui, const MenuFrame& frame) {
    const SettingPage& page = setting_pages()[page_];
    const float content = content_height_of(page);

    step_selection(page, frame.body);

    if (host_ != nullptr && page.tab != nullptr &&
        std::strcmp(page.tab, "FSR") == 0) {
        const Rect banner = {
            frame.body.x, frame.body.y, frame.body.width, theme::kRowHeight};
        ui.list->push_rect(banner, theme::kControl, theme::kControlRadius);
        draw_text_centered(
            *ui.list, *ui.font, banner, host_->upscale_status(), theme::kText);
    }

    const bool over_body =
        rect_contains(frame.body, ui.pointer.x, ui.pointer.y);
    if (over_body && ui.pointer.wheel != 0.0f) {
        scroll_ -= ui.pointer.wheel * kScrollStep;
    }
    scroll_ = clamp_scroll(scroll_, content, frame.body.height);

    UiContext body_ui = ui;
    body_ui.pointer.pressed = ui.pointer.pressed && over_body;

    ui.list->push_clip(frame.body);
    Rect rows = frame.body;
    rows.y -= scroll_;
    if (page.tab != nullptr && std::strcmp(page.tab, "FSR") == 0) {
        rows.y += theme::kRowHeight + theme::kRowGap;
    }
    rows.width -= kScrollBarWidth + theme::kRowGap;
    Layout layout(rows, theme::kRowHeight, theme::kRowGap);

    for (std::size_t index = 0; index < page.count; ++index) {
        const SettingBinding& binding = page.items[index];
        const Rect area = layout.row();
        if (area.y + area.height < frame.body.y ||
            area.y > frame.body.y + frame.body.height) {
            continue;
        }
        if (index == selected_) {
            const Rect highlight = {
                area.x - theme::kRowGap * 0.5f,
                area.y,
                area.width + theme::kRowGap,
                area.height};
            ui.list->push_rect(
                highlight, theme::kControl, theme::kControlRadius);
        }
        if (binding.kind == BindingKind::Toggle) {
            bool value = binding_flag(binding, *settings_);
            if (toggle_row(body_ui, area, binding.label, &value)) {
                set_binding_flag(binding, settings_, value);
                apply_change(binding);
            }
            continue;
        }
        float value = binding_value(binding, *settings_);
        const RowResult result = slider_row(body_ui, area, binding, &value);
        if (result.changed || (ui.pointer.pressed && over_body &&
                               rect_contains(area, ui.pointer.x, ui.pointer.y))) {
            selected_ = index;
        }
        if (result.changed) {
            set_binding_value(binding, settings_, value);
            apply_change(binding);
        }
        if (result.reset) {
            reset_binding(binding);
        }
    }
    ui.active = body_ui.active;
    ui.list->pop_clip();
    draw_scroll_bar(*ui.list, frame.body, scroll_, content);
}

}
}
