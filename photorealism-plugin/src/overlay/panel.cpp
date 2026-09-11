#include "layout.hpp"
#include "overlay.hpp"
#include "persistence.hpp"
#include "text.hpp"
#include "theme.hpp"
#include "widgets/controls.hpp"

#include <shellapi.h>

namespace photorealism {
namespace overlay {
namespace {

constexpr float kCloseSize = 22.0f;
constexpr float kTabHeight = 28.0f;
constexpr float kHeaderHeight = 26.0f;
constexpr float kFooterRows = 2.0f;

}

MenuFrame Menu::frame_for(float width, float height) const {
    const float available = height - theme::kPadding * 4.0f;
    const float panel_height = available < 760.0f ? available : 760.0f;

    MenuFrame frame = {};
    frame.panel.width = theme::kPanelWidth;
    frame.panel.height = panel_height;
    frame.panel.x = (width - frame.panel.width) * 0.5f;
    frame.panel.y = (height - frame.panel.height) * 0.5f;

    const float footer_height =
        kFooterRows * theme::kRowHeight + theme::kRowGap + theme::kPadding;
    const float body_top = frame.panel.y + theme::kTitleBarHeight +
                           theme::kPadding + kTabHeight + theme::kRowGap +
                           kHeaderHeight + theme::kRowGap;

    frame.body.x = frame.panel.x + theme::kPadding;
    frame.body.y = body_top;
    frame.body.width = frame.panel.width - theme::kPadding * 2.0f;
    frame.body.height =
        frame.panel.y + frame.panel.height - footer_height - body_top;

    frame.footer.x = frame.body.x;
    frame.footer.y = frame.panel.y + frame.panel.height - footer_height;
    frame.footer.width = frame.body.width;
    frame.footer.height = footer_height;
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

    const float tabs_top =
        panel.y + theme::kTitleBarHeight + theme::kPadding;
    const std::size_t count = setting_page_count();
    const float gap = theme::kRowGap;
    const float tab_width =
        (frame.body.width - gap * static_cast<float>(count - 1)) /
        static_cast<float>(count);
    for (std::size_t index = 0; index < count; ++index) {
        const Rect tab = {
            frame.body.x + (tab_width + gap) * static_cast<float>(index),
            tabs_top,
            tab_width,
            kTabHeight};
        tab_header(ui, tab, setting_pages()[index].tab, index == page_);
        if (ui.pointer.pressed && rect_contains(tab, ui.pointer.x, ui.pointer.y)) {
            page_ = index;
            scroll_ = 0.0f;
        }
    }

    const Rect header = {
        frame.body.x,
        tabs_top + kTabHeight + gap,
        frame.body.width,
        kHeaderHeight};
    draw_text(
        *ui.list,
        *ui.font,
        header.x,
        header.y + (header.height - ui.font->line_height()) * 0.5f,
        setting_pages()[page_].title,
        theme::kText);
    draw_text_right(
        *ui.list,
        *ui.font,
        header,
        "setas ajustam  enter reinicia  tab troca de aba",
        theme::kTextDim);
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
        ensure_baseline();
        save_settings(*settings_, baseline_, defaults_);
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
