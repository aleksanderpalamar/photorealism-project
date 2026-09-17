#pragma once

#include <d3d11.h>

namespace photorealism {

enum class EffectShader : unsigned {
    Fxaa,
    Occlusion,
    Compose,
    InteriorLight,
    Sharpen,
};

constexpr unsigned kEffectShaderCount = 5;

class EffectShaders {
  public:
    void compile(ID3D11Device* device);
    void release();

    ID3D11PixelShader* shader(EffectShader which) const {
        return shaders_[static_cast<unsigned>(which)];
    }
    bool available(EffectShader which) const { return shader(which) != nullptr; }

  private:
    ID3D11PixelShader* shaders_[kEffectShaderCount] = {};
};

}
