#pragma once

#include "../config/calibration.hpp"
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

constexpr const char* kMenuTitle = "photorealism-plugin 0.21.2";
constexpr const char* kProjectUrl =
    "https://github.com/aleksanderpalamar/photorealism-project";

class MenuHost {
  public:
    virtual ~MenuHost() = default;
    virtual void settings_changed(const SettingBinding& binding) = 0;
};

struct MenuFrame {
    Rect panel;
    Rect body;
    Rect footer;
    float content_height;
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
    void ensure_baseline();
    MenuFrame frame_for(float width, float height) const;
    void draw_chrome(UiContext& ui, const MenuFrame& frame);
    void draw_footer(UiContext& ui, const MenuFrame& frame);
    void draw_body(UiContext& ui, const MenuFrame& frame);
    void step_selection(const SettingPage& page, const Rect& body);
    void apply_change(const SettingBinding& binding);
    void reset_binding(const SettingBinding& binding);
    void discard_changes();

    Renderer renderer_;
    BakedFont font_;
    BakedFontView view_;
    DrawList list_;
    Settings* settings_ = nullptr;
    MenuHost* host_ = nullptr;
    Settings baseline_ = {};
    Settings defaults_ = {};
    Settings on_disk_ = {};
    ID3D11Device* device_ = nullptr;
    const void* active_ = nullptr;
    std::size_t page_ = 0;
    std::size_t selected_ = 0;
    unsigned keys_ = 0;
    float scroll_ = 0.0f;
    bool baseline_loaded_ = false;
    bool visible_ = false;
    bool close_armed_ = false;
};

Menu& menu();

}
}
