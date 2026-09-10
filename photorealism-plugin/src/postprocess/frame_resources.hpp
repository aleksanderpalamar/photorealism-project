#pragma once

#include <d3d11.h>

namespace photorealism {

class FrameResources {
  public:
    void attach(ID3D11Device* device);
    void release();

    bool matches(const D3D11_TEXTURE2D_DESC& source) const;
    bool create(const D3D11_TEXTURE2D_DESC& source);

    ID3D11Texture2D* scene_texture() const { return scene_texture_; }
    ID3D11ShaderResourceView* scene_view() const { return scene_view_; }
    ID3D11ShaderResourceView* visual_view() const { return visual_view_; }
    ID3D11RenderTargetView* visual_target() const { return visual_target_; }
    ID3D11ShaderResourceView* spatial_view() const { return spatial_view_; }
    ID3D11RenderTargetView* spatial_target() const { return spatial_target_; }
    bool scene_needs_srgb_decode() const { return scene_needs_srgb_decode_; }

  private:
    struct IntermediateTarget {
        ID3D11Texture2D* texture;
        ID3D11ShaderResourceView* view;
        ID3D11RenderTargetView* target;
    };

    void release_intermediate_target(IntermediateTarget* intermediate);
    D3D11_TEXTURE2D_DESC intermediate_description(
        const D3D11_TEXTURE2D_DESC& source, UINT bind_flags) const;
    HRESULT create_srgb_view(
        ID3D11Texture2D* texture,
        const D3D11_TEXTURE2D_DESC& source,
        ID3D11ShaderResourceView** view);
    HRESULT create_srgb_target(
        ID3D11Texture2D* texture,
        const D3D11_TEXTURE2D_DESC& source,
        ID3D11RenderTargetView** target);
    HRESULT create_intermediate_target(
        const D3D11_TEXTURE2D_DESC& source, IntermediateTarget* intermediate);
    static bool intermediate_is_complete(
        HRESULT result, const IntermediateTarget& intermediate);
    bool create_scene_texture(const D3D11_TEXTURE2D_DESC& source);
    void create_visual_target(const D3D11_TEXTURE2D_DESC& source);
    void create_spatial_target(const D3D11_TEXTURE2D_DESC& source);
    void release_scene_textures();

    ID3D11Device* device_ = nullptr;
    ID3D11Texture2D* scene_texture_ = nullptr;
    ID3D11ShaderResourceView* scene_view_ = nullptr;
    ID3D11Texture2D* visual_texture_ = nullptr;
    ID3D11ShaderResourceView* visual_view_ = nullptr;
    ID3D11RenderTargetView* visual_target_ = nullptr;
    ID3D11Texture2D* spatial_texture_ = nullptr;
    ID3D11ShaderResourceView* spatial_view_ = nullptr;
    ID3D11RenderTargetView* spatial_target_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    bool scene_needs_srgb_decode_ = false;
    bool input_fallback_logged_ = false;
    bool visual_failure_logged_ = false;
    bool spatial_failure_logged_ = false;
};

}  // namespace photorealism
