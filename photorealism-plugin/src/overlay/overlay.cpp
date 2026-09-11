#include "overlay.hpp"

#include "../runtime.hpp"
#include "font_bitmap.hpp"
#include "input.hpp"
#include "text.hpp"
#include "theme.hpp"

namespace photorealism {
namespace overlay {
namespace {

constexpr int kEmbeddedScale = 2;
constexpr float kCloseSize = 22.0f;
constexpr float kPointerHeight = 13.0f;

void draw_border(DrawList& list, const Rect& area, const Color& color, float radius) {
    list.push_rect(area, color, radius);
}

void draw_pointer(DrawList& list, float x, float y) {
    for (int row = 0; row < static_cast<int>(kPointerHeight); ++row) {
        const float width = static_cast<float>(row) + 1.0f;
        const Rect edge = {x - 1.0f, y + static_cast<float>(row) - 1.0f, width + 2.0f, 3.0f};
        list.push_rect(edge, theme::kCursorEdge, 0.0f);
    }
    for (int row = 0; row < static_cast<int>(kPointerHeight); ++row) {
        const float width = static_cast<float>(row) + 1.0f;
        const Rect body = {x, y + static_cast<float>(row), width, 1.0f};
        list.push_rect(body, theme::kCursor, 0.0f);
    }
}

}

Menu& menu() {
    static Menu instance;
    return instance;
}

void Menu::toggle() {
    visible_ = !visible_;
    close_armed_ = false;
    input_hook().set_capturing(visible_);
    log_message("Menu %s pelo atalho Ctrl+P.", visible_ ? "aberto" : "fechado");
}

void Menu::hide() {
    if (!visible_) {
        return;
    }
    visible_ = false;
    close_armed_ = false;
    input_hook().set_capturing(false);
    log_message("Menu fechado pelo botao de fechar.");
}

void Menu::release() {
    renderer_.release();
    view_.reset(nullptr);
    device_ = nullptr;
}

bool Menu::ensure_resources(ID3D11Device* device) {
    if (device == nullptr) {
        return false;
    }
    if (renderer_.ready() && device_ == device) {
        return true;
    }
    release();

    font_ = bake_embedded_font(kEmbeddedScale);
    view_.reset(&font_);
    if (!renderer_.create(device, font_)) {
        log_message("Menu nao pode ser desenhado: renderer indisponivel.");
        return false;
    }
    device_ = device;
    log_message(
        "Menu pronto: fonte embutida %dx%d, %u glifos.",
        font_.width,
        font_.height,
        kGlyphCount);
    return true;
}

Rect Menu::panel_rect(float width, float height) const {
    Rect panel = {};
    panel.width = theme::kPanelWidth;
    panel.height = kMenuHeight;
    panel.x = (width - panel.width) * 0.5f;
    panel.y = (height - panel.height) * 0.5f;
    return panel;
}

void Menu::build(const PointerState& pointer, float width, float height) {
    const Rect panel = panel_rect(width, height);
    draw_border(
        list_,
        rect_inset(panel, -theme::kBorderWidth),
        theme::kPanelBorder,
        theme::kCornerRadius + theme::kBorderWidth);
    list_.push_rect(panel, theme::kPanel, theme::kCornerRadius);

    const Rect title_bar = {
        panel.x, panel.y, panel.width, theme::kTitleBarHeight};
    list_.push_rect(title_bar, theme::kTitleBar, theme::kCornerRadius);
    const Rect title_foot = {
        panel.x,
        panel.y + theme::kTitleBarHeight * 0.5f,
        panel.width,
        theme::kTitleBarHeight * 0.5f};
    list_.push_rect(title_foot, theme::kTitleBar, 0.0f);

    const Rect title_text = {
        panel.x + theme::kPadding,
        panel.y,
        panel.width - theme::kPadding * 2.0f - kCloseSize,
        theme::kTitleBarHeight};
    draw_text(
        list_,
        view_,
        title_text.x,
        title_text.y + (title_text.height - view_.line_height()) * 0.5f,
        kMenuTitle,
        theme::kText);

    const Rect close = {
        panel.x + panel.width - theme::kPadding - kCloseSize,
        panel.y + (theme::kTitleBarHeight - kCloseSize) * 0.5f,
        kCloseSize,
        kCloseSize};
    const bool over_close = rect_contains(close, pointer.x, pointer.y);
    list_.push_rect(
        close,
        over_close ? theme::kDanger : theme::kControl,
        theme::kControlRadius);
    draw_text_centered(list_, view_, close, "x", theme::kText);

    const Rect separator = {
        panel.x,
        panel.y + theme::kTitleBarHeight,
        panel.width,
        theme::kSeparatorHeight};
    list_.push_rect(separator, theme::kSeparator, 0.0f);

    float row = panel.y + theme::kTitleBarHeight + theme::kPadding;
    const char* lines[] = {
        "Menu do plugin em construcao.",
        "Ctrl+P abre e fecha.",
        "Clique no x para fechar.",
    };
    for (const char* line : lines) {
        draw_text(
            list_, view_, panel.x + theme::kPadding, row, line, theme::kTextDim);
        row += view_.line_height() + theme::kRowGap;
    }

    if (over_close && pointer.pressed) {
        close_armed_ = true;
    }
    if (close_armed_ && pointer.released) {
        close_armed_ = false;
        if (over_close) {
            hide();
        }
    }

    if (pointer.inside) {
        draw_pointer(list_, pointer.x, pointer.y);
    }
}

void Menu::render(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* target,
    HWND window,
    float width,
    float height,
    bool output_needs_srgb_encode) {
    if (!visible_) {
        return;
    }
    if (input_hook().install(window)) {
        input_hook().set_capturing(true);
    }
    if (!ensure_resources(device)) {
        return;
    }

    const PointerState pointer = input_hook().poll();
    list_.begin(width, height);
    build(pointer, width, height);
    renderer_.draw(context, target, list_, output_needs_srgb_encode);
}

}
}
