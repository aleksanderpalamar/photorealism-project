#pragma once

#include "draw_list.hpp"
#include "font.hpp"

#include <cstddef>
#include <d3d11.h>

namespace photorealism {
namespace overlay {

constexpr std::size_t kInitialVertexCapacity = 4096;

struct OverlayConstants {
    float output_needs_srgb_encode;
    float padding[3];
};

class Renderer {
  public:
    bool create(ID3D11Device* device, const BakedFont& font);
    void release();
    bool ready() const { return vertex_shader_ != nullptr && font_view_ != nullptr; }

    void draw(
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* target,
        const DrawList& list,
        bool output_needs_srgb_encode);

  private:
    bool create_shaders(ID3D11Device* device);
    bool create_states(ID3D11Device* device, bool smooth);
    bool create_constants(ID3D11Device* device);
    bool create_font_texture(ID3D11Device* device, const BakedFont& font);
    bool ensure_capacity(std::size_t vertex_count);
    bool upload(ID3D11DeviceContext* context, const DrawList& list);
    void bind(
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* target,
        const DrawList& list);

    ID3D11Device* device_ = nullptr;
    ID3D11VertexShader* vertex_shader_ = nullptr;
    ID3D11PixelShader* pixel_shader_ = nullptr;
    ID3D11Buffer* vertices_ = nullptr;
    ID3D11ShaderResourceView* vertex_view_ = nullptr;
    ID3D11Buffer* constants_ = nullptr;
    ID3D11BlendState* blend_ = nullptr;
    ID3D11DepthStencilState* depth_ = nullptr;
    ID3D11RasterizerState* rasterizer_ = nullptr;
    ID3D11SamplerState* sampler_ = nullptr;
    ID3D11Texture2D* font_texture_ = nullptr;
    ID3D11ShaderResourceView* font_view_ = nullptr;
    std::size_t capacity_ = 0;
};

}
}
