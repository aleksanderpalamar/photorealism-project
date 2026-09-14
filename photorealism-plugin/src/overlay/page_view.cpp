#include "layout.hpp"
#include "overlay.hpp"
#include "page_metrics.hpp"
#include "text.hpp"
#include "theme.hpp"
#include "widgets/controls.hpp"

namespace photorealism {
namespace overlay {
namespace {

constexpr float kScrollStep = 48.0f;
constexpr float kScrollBarWidth = 4.0f;

float clamp_scroll(float scroll, float content, float view) {
    const float limit = content - view;
    if (limit <= 0.0f || scroll < 0.0f) {
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

bool outside(const Rect& area, const Rect& body) {
    return area.y + area.height < body.y || area.y > body.y + body.height;
}

}

void Menu::open_page(std::size_t page) {
    next_page_ = page < setting_page_count() ? page : kPageMain;
}

void Menu::apply_navigation() {
    if (next_page_ == kNoPage) {
        return;
    }
    page_ = next_page_;
    next_page_ = kNoPage;
    selected_ = 0;
    column_ = 0;
    scroll_ = 0.0f;
    active_ = nullptr;
    open_choice_ = nullptr;
}

void Menu::draw_body(UiContext& ui, const MenuFrame& frame) {
    apply_navigation();
    ui.active = active_;
    const SettingPage& page = setting_pages()[page_];
    const float content = content_height_of(page);
    step_selection(page, frame.body);

    const bool over_body = rect_contains(frame.body, ui.pointer.x, ui.pointer.y);
    if (over_body && ui.pointer.wheel != 0.0f && open_choice_ == nullptr) {
        scroll_ -= ui.pointer.wheel * kScrollStep;
    }
    scroll_ = clamp_scroll(scroll_, content, frame.body.height);

    const bool over_popup =
        open_choice_ != nullptr &&
        rect_contains(open_popup_, ui.pointer.x, ui.pointer.y);
    UiContext body_ui = ui;
    body_ui.pointer.pressed = ui.pointer.pressed && over_body && !over_popup;
    choice_clicked_ = false;

    ui.list->push_clip(frame.body);
    Rect rows = frame.body;
    rows.y -= scroll_;
    const bool scrolls = content > frame.body.height;
    rows.width -= scrolls ? kScrollBarWidth + theme::kRowGap : 0.0f;
    if (page.upscale_status && host_ != nullptr) {
        const Rect banner = {rows.x, rows.y, rows.width, theme::kRowHeight};
        ui.list->push_rect(banner, theme::kControl, theme::kControlRadius);
        draw_text_centered(
            *ui.list, *ui.font, banner, host_->upscale_status(), theme::kText);
    }
    rows.y += banner_height(page);
    Layout layout(rows, theme::kRowHeight, theme::kRowGap);

    bool popup_shown = false;
    for (std::size_t index = 0; index < page.count; ++index) {
        const MenuRow& row = page.rows[index];
        const Rect area = layout.row(row_height(row));
        if (outside(area, frame.body)) {
            continue;
        }
        popup_shown = draw_row(body_ui, row, area, index, frame.body) || popup_shown;
    }
    ui.active = body_ui.active;
    ui.list->pop_clip();
    draw_scroll_bar(*ui.list, frame.body, scroll_, content);
    draw_open_choice(ui, popup_shown);
}

}
}
