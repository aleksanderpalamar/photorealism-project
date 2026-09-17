#pragma once

#include "../config/effect_quality.hpp"

#include <d3d11.h>

namespace photorealism {
namespace passfx {

class PreToneEffect {
  public:
    bool create(ID3D11Device* device);
    void release();
    bool ready() const { return shader_ != nullptr && constants_ != nullptr; }

    bool apply(
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* hdr,
        ID3D11VertexShader* vertex_shader,
        const PreToneParameters& parameters);

  private:
    bool ensure_scratch(ID3D11Texture2D* source);
    void draw(
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* hdr,
        ID3D11VertexShader* vertex_shader,
        const PreToneParameters& parameters);

    ID3D11Device* device_ = nullptr;
    ID3D11PixelShader* shader_ = nullptr;
    ID3D11Buffer* constants_ = nullptr;
    ID3D11BlendState* blend_ = nullptr;
    ID3D11DepthStencilState* depth_ = nullptr;
    ID3D11RasterizerState* rasterizer_ = nullptr;
    ID3D11Texture2D* scratch_ = nullptr;
    ID3D11ShaderResourceView* scratch_view_ = nullptr;
    D3D11_TEXTURE2D_DESC scratch_description_ = {};
};

}
}
