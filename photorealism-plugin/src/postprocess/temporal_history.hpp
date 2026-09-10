#pragma once

#include <d3d11.h>

#include <cstdint>

namespace photorealism {

class TemporalHistory {
  public:
    void attach(ID3D11Device* device, ID3D11DeviceContext* context);
    void release();

    bool ensure(
        const D3D11_TEXTURE2D_DESC& frame_description,
        const D3D11_TEXTURE2D_DESC& depth_description,
        std::uint64_t generation,
        bool temporal_shader_available,
        ID3D11Texture2D* depth_copy_texture);

    void store(
        ID3D11Texture2D* back_buffer, ID3D11Texture2D* depth_copy_texture);

    bool valid() const { return valid_; }
    void mark_valid() { valid_ = true; }
    bool invalidate();

    ID3D11ShaderResourceView* color_view() const { return color_view_; }
    ID3D11ShaderResourceView* depth_view() const { return depth_view_; }
    bool failure_logged() const { return failure_logged_; }
    void set_failure_logged(bool value) { failure_logged_ = value; }

  private:
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    ID3D11Texture2D* color_texture_ = nullptr;
    ID3D11ShaderResourceView* color_view_ = nullptr;
    ID3D11Texture2D* depth_texture_ = nullptr;
    ID3D11ShaderResourceView* depth_view_ = nullptr;
    std::uint64_t generation_ = 0;
    UINT depth_width_ = 0;
    UINT depth_height_ = 0;
    bool valid_ = false;
    bool failure_logged_ = false;
};

}  // namespace photorealism
