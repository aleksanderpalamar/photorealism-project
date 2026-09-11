#pragma once

#include "draw_list.hpp"
#include "font.hpp"
#include "input_state.hpp"
#include "renderer.hpp"

#include <d3d11.h>
#include <windows.h>

namespace photorealism {
namespace overlay {

constexpr const char* kMenuTitle = "photorealism-plugin 0.20.0";
constexpr float kMenuHeight = 420.0f;

class Menu {
  public:
    void toggle();
    void hide();
    void release();

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
    void build(const PointerState& pointer, float width, float height);
    Rect panel_rect(float width, float height) const;

    Renderer renderer_;
    BakedFont font_;
    BakedFontView view_;
    DrawList list_;
    ID3D11Device* device_ = nullptr;
    bool visible_ = false;
    bool close_armed_ = false;
};

Menu& menu();

}
}
