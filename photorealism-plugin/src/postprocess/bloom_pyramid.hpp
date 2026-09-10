#pragma once

#include "shader_constants.hpp"
#include "shader_library.hpp"

#include <d3d11.h>

namespace photorealism {

struct BloomFrame {
    const ShaderLibrary* shaders;
    ID3D11SamplerState* sampler;
    ID3D11ShaderResourceView* scene_view;
    ID3D11BlendState* additive_blend;
    ID3D11BlendState* opaque_blend;
    bool scene_needs_srgb_decode;
    float threshold;
    float knee;
};

class BloomPyramid {
  public:
    static constexpr UINT kMaxLevels = 6;

    bool attach(ID3D11Device* device, ID3D11DeviceContext* context);
    void release();
    void release_constant_buffer();

    static UINT levels_for_radius(float radius, UINT height);
    bool ensure(const D3D11_TEXTURE2D_DESC& source, UINT requested_levels);
    void render(const D3D11_TEXTURE2D_DESC& description, const BloomFrame& frame);
    void draw_preview(
        ID3D11RenderTargetView* output,
        const BloomFrame& frame,
        bool output_needs_srgb_encode);

    UINT level_count() const { return level_count_; }
    ID3D11ShaderResourceView* top_view() const { return views_[0]; }
    bool failure_logged() const { return failure_logged_; }
    void set_failure_logged(bool value) { failure_logged_ = value; }

  private:
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    ID3D11Buffer* constant_buffer_ = nullptr;
    ID3D11Texture2D* textures_[kMaxLevels] = {};
    ID3D11ShaderResourceView* views_[kMaxLevels] = {};
    ID3D11RenderTargetView* targets_[kMaxLevels] = {};
    UINT widths_[kMaxLevels] = {};
    UINT heights_[kMaxLevels] = {};
    UINT level_count_ = 0;
    bool failure_logged_ = false;
};

}  // namespace photorealism
