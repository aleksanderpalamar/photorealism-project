#pragma once

#include "../config/settings.hpp"
#include "draw_list.hpp"
#include "font.hpp"
#include "input_state.hpp"
#include "renderer.hpp"
#include "widgets/controls.hpp"

#include <cstddef>
#include <d3d11.h>
#include <windows.h>

namespace photorealism {
namespace overlay {

constexpr const char* kMenuTitle = "photorealism-plugin 0.24.1";
constexpr const char* kProjectUrl =
    "https://github.com/aleksanderpalamar/photorealism-project";
constexpr std::size_t kNoPage = static_cast<std::size_t>(-1);

class MenuHost {
  public:
    virtual ~MenuHost() = default;
    virtual void settings_changed(const SettingBinding& binding) = 0;
    virtual void settings_reloaded() = 0;
    virtual const char* upscale_status() const = 0;
    virtual void request_frame_capture() = 0;
    virtual const char* frame_capture_status() const = 0;
};

struct MenuFrame {
    Rect panel;
    Rect header;
    Rect body;
    Rect footer;
};

class Menu {
  public:
    void toggle();
    void hide();
    void release();
    void bind(Settings* settings, MenuHost* host);

    bool visible() const { return visible_; }

    void render(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* target,
        HWND window,
        float width,
        float height,
        bool output_needs_srgb_encode);

  private:
    bool ensure_resources(ID3D11Device* device);
    void ensure_on_disk();
    MenuFrame frame_for(float width, float height) const;
    void draw_chrome(UiContext& ui, const MenuFrame& frame);
    void draw_footer(UiContext& ui, const MenuFrame& frame);
    void draw_body(UiContext& ui, const MenuFrame& frame);
    void apply_navigation();
    void open_page(std::size_t page);
    void step_selection(const SettingPage& page, const Rect& body);
    bool step_pair_column(const MenuRow& row);
    void step_binding(const SettingBinding& binding);
    void activate_row(const MenuRow& row);
    bool draw_row(
        UiContext& ui,
        const MenuRow& row,
        const Rect& area,
        std::size_t index,
        const Rect& body);
    bool draw_pair(
        UiContext& ui, const MenuRow& row, const Rect& area, const Rect& body);
    bool draw_setting(
        UiContext& ui,
        const SettingBinding& binding,
        const Rect& area,
        const Rect& body);
    void draw_toggle(
        UiContext& ui, const SettingBinding& binding, const Rect& area);
    void draw_slider(
        UiContext& ui, const SettingBinding& binding, const Rect& area);
    bool draw_choice(
        UiContext& ui,
        const SettingBinding& binding,
        const Rect& area,
        const Rect& body);
    void draw_open_choice(UiContext& ui, bool shown);
    void set_value(const SettingBinding& binding, float value);
    void apply_change(const SettingBinding& binding);
    void reset_binding(const SettingBinding& binding);
    void discard_changes();
    void restore_defaults();

    Renderer renderer_;
    BakedFont font_;
    BakedFontView view_;
    DrawList list_;
    Settings* settings_ = nullptr;
    MenuHost* host_ = nullptr;
    Settings on_disk_ = {};
    ID3D11Device* device_ = nullptr;
    const void* active_ = nullptr;
    const SettingBinding* open_choice_ = nullptr;
    Rect open_popup_ = {};
    bool choice_clicked_ = false;
    std::size_t page_ = 0;
    std::size_t next_page_ = kNoPage;
    std::size_t selected_ = 0;
    std::size_t column_ = 0;
    unsigned keys_ = 0;
    float scroll_ = 0.0f;
    bool on_disk_loaded_ = false;
    bool visible_ = false;
    bool close_armed_ = false;
};

Menu& menu();

}
}
