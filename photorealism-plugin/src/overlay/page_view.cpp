#include "layout.hpp"
#include "overlay.hpp"
#include "text.hpp"
#include "theme.hpp"
#include "widgets/controls.hpp"

namespace photorealism {
namespace overlay {
namespace {

constexpr float kScrollStep = 48.0f;
constexpr float kScrollBarWidth = 4.0f;

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

void Menu::draw_body(UiContext& ui, const MenuFrame& frame) {
    const SettingPage& page = setting_pages()[page_];
    const float content = content_height_of(page);

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
    rows.width -= kScrollBarWidth + theme::kRowGap;
    Layout layout(rows, theme::kRowHeight, theme::kRowGap);

    for (std::size_t index = 0; index < page.count; ++index) {
        const SettingBinding& binding = page.items[index];
        const Rect area = layout.row();
        if (area.y + area.height < frame.body.y ||
            area.y > frame.body.y + frame.body.height) {
            continue;
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
