#pragma once

#include <d3d11.h>

namespace photorealism {

struct IntermediateTarget {
    ID3D11Texture2D* texture;
    ID3D11ShaderResourceView* view;
    ID3D11ShaderResourceView* raw_view;
    ID3D11RenderTargetView* target;
};

class FrameResources {
  public:
    void attach(ID3D11Device* device);
    void release();

    bool matches(const D3D11_TEXTURE2D_DESC& source) const;
    bool create(const D3D11_TEXTURE2D_DESC& source);

    ID3D11Texture2D* scene_texture() const { return scene_texture_; }
    ID3D11ShaderResourceView* scene_view() const { return scene_view_; }
    const IntermediateTarget& first() const { return first_; }
    const IntermediateTarget& second() const { return second_; }
    bool intermediates_ready() const {
        return first_.target != nullptr && second_.target != nullptr;
    }
    bool scene_needs_srgb_decode() const { return scene_needs_srgb_decode_; }

  private:
    D3D11_TEXTURE2D_DESC intermediate_description(
        const D3D11_TEXTURE2D_DESC& source, UINT bind_flags) const;
    HRESULT create_view(
        ID3D11Texture2D* texture, DXGI_FORMAT format,
        ID3D11ShaderResourceView** view);
    HRESULT create_intermediate(
        const D3D11_TEXTURE2D_DESC& source, IntermediateTarget* intermediate);
    bool create_scene_texture(const D3D11_TEXTURE2D_DESC& source);
    void create_named_intermediate(
        const D3D11_TEXTURE2D_DESC& source, IntermediateTarget* intermediate,
        const char* name);
    void release_scene_textures();

    ID3D11Device* device_ = nullptr;
    ID3D11Texture2D* scene_texture_ = nullptr;
    ID3D11ShaderResourceView* scene_view_ = nullptr;
    IntermediateTarget first_ = {};
    IntermediateTarget second_ = {};
    UINT width_ = 0;
    UINT height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    bool scene_needs_srgb_decode_ = false;
    bool input_fallback_logged_ = false;
    bool intermediate_failure_logged_ = false;
};

}
