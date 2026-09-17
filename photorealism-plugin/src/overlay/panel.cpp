#include "layout.hpp"
#include "overlay.hpp"
#include "page_metrics.hpp"
#include "persistence.hpp"
#include "text.hpp"
#include "theme.hpp"
#include "widgets/controls.hpp"

#include <shellapi.h>

namespace photorealism {
namespace overlay {
namespace {

constexpr float kCloseSize = 22.0f;
constexpr float kHeaderHeight = 26.0f;
constexpr float kFooterRows = 2.0f;
constexpr float kScreenMargin = theme::kPadding * 2.0f;

}

MenuFrame Menu::frame_for(float height) const {
    const float footer_height =
        kFooterRows * theme::kRowHeight + theme::kRowGap + theme::kPadding;
    const float chrome_height = theme::kTitleBarHeight + theme::kPadding +
                                kHeaderHeight + theme::kRowGap +
                                theme::kRowGap * 2.0f + footer_height;
    const float content =
        content_height_of(setting_pages()[page_]) + theme::kPadding;
    const float available = height - kScreenMargin - theme::kPadding * 2.0f;
    const float wanted = chrome_height + content;
    const float panel_height = wanted < available ? wanted : available;

    MenuFrame frame = {};
    frame.panel.width = theme::kPanelWidth;
    frame.panel.height = panel_height;
    frame.panel.x = kScreenMargin;
    frame.panel.y = kScreenMargin;

    const float inner_x = frame.panel.x + theme::kPadding;
    const float inner_width = frame.panel.width - theme::kPadding * 2.0f;
    frame.header = {
        inner_x,
        frame.panel.y + theme::kTitleBarHeight + theme::kPadding,
        inner_width,
        kHeaderHeight};
    frame.body.x = inner_x;
    frame.body.y = frame.header.y + kHeaderHeight + theme::kRowGap + 2.0f;
    frame.body.width = inner_width;
    frame.footer = {
        inner_x,
        frame.panel.y + frame.panel.height - footer_height,
        inner_width,
        footer_height};
    frame.body.height = frame.footer.y - theme::kRowGap * 2.0f - frame.body.y;
    return frame;
}

void Menu::draw_chrome(UiContext& ui, const MenuFrame& frame) {
    const Rect& panel = frame.panel;
    ui.list->push_rect(
        rect_inset(panel, -theme::kBorderWidth),
        theme::kPanelBorder,
        theme::kCornerRadius + theme::kBorderWidth);
    ui.list->push_rect(panel, theme::kPanel, theme::kCornerRadius);

    const Rect title_bar = {
        panel.x, panel.y, panel.width, theme::kTitleBarHeight};
    ui.list->push_rect(title_bar, theme::kTitleBar, theme::kCornerRadius);
    const Rect title_foot = {
        panel.x,
        panel.y + theme::kTitleBarHeight * 0.5f,
        panel.width,
        theme::kTitleBarHeight * 0.5f};
    ui.list->push_rect(title_foot, theme::kTitleBar, 0.0f);
    draw_text(
        *ui.list,
        *ui.font,
        panel.x + theme::kPadding,
        panel.y + (theme::kTitleBarHeight - ui.font->line_height()) * 0.5f,
        kMenuTitle,
        theme::kText);

    const Rect close = {
        panel.x + panel.width - theme::kPadding - kCloseSize,
        panel.y + (theme::kTitleBarHeight - kCloseSize) * 0.5f,
        kCloseSize,
        kCloseSize};
    const bool over_close = rect_contains(close, ui.pointer.x, ui.pointer.y);
    ui.list->push_rect(
        close,
        over_close ? theme::kDanger : theme::kControl,
        theme::kControlRadius);
    draw_text_centered(*ui.list, *ui.font, close, "x", theme::kText);
    if (over_close && ui.pointer.pressed) {
        close_armed_ = true;
    }
    if (close_armed_ && ui.pointer.released) {
        close_armed_ = false;
        if (over_close) {
            hide();
        }
    }

    draw_text_centered(
        *ui.list, *ui.font, frame.header, setting_pages()[page_].title,
        theme::kText);
}

void Menu::draw_footer(UiContext& ui, const MenuFrame& frame) {
    const Rect separator = {
        frame.panel.x,
        frame.footer.y - theme::kRowGap,
        frame.panel.width,
        theme::kSeparatorHeight};
    ui.list->push_rect(separator, theme::kSeparator, 0.0f);

    Layout layout(frame.footer, theme::kRowHeight, theme::kRowGap);
    const Rect first = layout.row();
    const float half = (first.width - theme::kRowGap) * 0.5f;
    const Rect save = {first.x, first.y, half, first.height};
    const Rect discard = {
        first.x + half + theme::kRowGap, first.y, half, first.height};
    if (button(ui, save, "Salvar no cfg", true)) {
        ensure_on_disk();
        const SaveReport report = save_settings(*settings_, on_disk_);
        if (report.written) {
            on_disk_ = *settings_;
        }
    }
    if (button(ui, discard, "Descartar mudancas", false)) {
        discard_changes();
    }
    if (button(ui, layout.row(), kProjectUrl, false)) {
        ShellExecuteA(nullptr, "open", kProjectUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

}
}
