#include "overlay.hpp"

#include "../config/config.hpp"
#include "../config/limits.hpp"
#include "../runtime.hpp"
#include "font_bitmap.hpp"
#include "input.hpp"
#include "persistence.hpp"
#include "theme.hpp"

namespace photorealism {
namespace overlay {
namespace {

constexpr int kEmbeddedScale = 2;

void draw_pointer(DrawList& list, float x, float y) {
    constexpr int kHeight = 13;
    for (int row = 0; row < kHeight; ++row) {
        const float width = static_cast<float>(row) + 1.0f;
        const Rect edge = {
            x - 1.0f, y + static_cast<float>(row) - 1.0f, width + 2.0f, 3.0f};
        list.push_rect(edge, theme::kCursorEdge, 0.0f);
    }
    for (int row = 0; row < kHeight; ++row) {
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

void Menu::bind(Settings* settings, MenuHost* host) {
    settings_ = settings;
    host_ = host;
    baseline_loaded_ = false;
}

void Menu::toggle() {
    visible_ = !visible_;
    close_armed_ = false;
    active_ = nullptr;
    input_hook().set_capturing(visible_);
    log_message("Menu %s pelo atalho Ctrl+P.", visible_ ? "aberto" : "fechado");
}

void Menu::hide() {
    if (!visible_) {
        return;
    }
    visible_ = false;
    close_armed_ = false;
    active_ = nullptr;
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
        "Menu pronto: fonte embutida %dx%d, %u glifos, %u paginas.",
        font_.width,
        font_.height,
        kGlyphCount,
        static_cast<unsigned>(setting_page_count()));
    return true;
}

void Menu::ensure_baseline() {
    if (baseline_loaded_) {
        return;
    }
    CalibrationStack stack = {};
    load_stack(&stack);
    baseline_ = measured_baseline(stack);
    defaults_ = default_settings();
    baseline_loaded_ = true;
}

void Menu::apply_change(const SettingBinding& binding) {
    apply_limits(settings_);
    if (host_ != nullptr && binding_touches_observer(binding)) {
        host_->observer_changed();
    }
}

void Menu::reset_binding(const SettingBinding& binding) {
    ensure_baseline();
    const Settings& reference = binding.grade ? baseline_ : defaults_;
    if (binding.kind == BindingKind::Toggle) {
        set_binding_flag(binding, settings_, binding_flag(binding, reference));
    } else {
        set_binding_value(
            binding, settings_, binding_value(binding, reference));
    }
    apply_change(binding);
    log_message("Menu devolveu %s ao valor de referencia.", binding.label);
}

void Menu::discard_changes() {
    if (settings_ == nullptr) {
        return;
    }
    load_settings(settings_);
    baseline_loaded_ = false;
    ensure_baseline();
    if (host_ != nullptr) {
        host_->observer_changed();
    }
    log_message("Menu descartou as mudancas e releu o cfg.");
}

void Menu::render(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* target,
    HWND window,
    float width,
    float height,
    bool output_needs_srgb_encode) {
    if (!visible_ || settings_ == nullptr) {
        return;
    }
    if (input_hook().install(window)) {
        input_hook().set_capturing(true);
    }
    if (!ensure_resources(device)) {
        return;
    }
    ensure_baseline();

    UiContext ui;
    ui.list = &list_;
    ui.font = &view_;
    ui.pointer = input_hook().poll();
    ui.active = active_;

    const MenuFrame frame = frame_for(width, height);
    list_.begin(width, height);
    draw_chrome(ui, frame);
    draw_body(ui, frame);
    draw_footer(ui, frame);
    if (ui.pointer.inside) {
        draw_pointer(list_, ui.pointer.x, ui.pointer.y);
    }
    active_ = ui.active;

    renderer_.draw(context, target, list_, output_needs_srgb_encode);
}

}
}
