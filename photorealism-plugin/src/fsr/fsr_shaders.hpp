#pragma once

#include <d3d11.h>

namespace photorealism {
namespace fsr {

class FsrShaders {
  public:
    bool create(ID3D11Device* device);
    void release();

    bool ready() const {
        return easu_ != nullptr && rcas_vertex_ != nullptr &&
               rcas_pixel_ != nullptr;
    }

    ID3D11ComputeShader* easu() const { return easu_; }
    ID3D11VertexShader* rcas_vertex() const { return rcas_vertex_; }
    ID3D11PixelShader* rcas_pixel() const { return rcas_pixel_; }
    ID3D11Buffer* easu_constants() const { return easu_constants_; }
    ID3D11Buffer* rcas_constants() const { return rcas_constants_; }
    ID3D11SamplerState* sampler() const { return sampler_; }

  private:
    bool create_shaders(ID3D11Device* device);
    bool create_buffers(ID3D11Device* device);

    ID3D11ComputeShader* easu_ = nullptr;
    ID3D11VertexShader* rcas_vertex_ = nullptr;
    ID3D11PixelShader* rcas_pixel_ = nullptr;
    ID3D11Buffer* easu_constants_ = nullptr;
    ID3D11Buffer* rcas_constants_ = nullptr;
    ID3D11SamplerState* sampler_ = nullptr;
};

}
}
