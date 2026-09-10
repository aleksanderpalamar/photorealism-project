#pragma once

#include <d3d11.h>

#include <cstdint>

namespace photorealism {

class DepthCapture {
  public:
    void attach(ID3D11Device* device, ID3D11DeviceContext* context);
    void release();

    bool ensure(
        ID3D11Texture2D* source,
        const D3D11_TEXTURE2D_DESC& source_description,
        std::uint64_t generation,
        bool shader_available);
    void copy_from(ID3D11Texture2D* source);

    ID3D11Texture2D* texture() const { return texture_; }
    ID3D11ShaderResourceView* view() const { return view_; }

  private:
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    ID3D11Texture2D* texture_ = nullptr;
    ID3D11ShaderResourceView* view_ = nullptr;
    std::uint64_t candidate_generation_ = 0;
    std::uint64_t failed_generation_ = 0;
};

}  // namespace photorealism
