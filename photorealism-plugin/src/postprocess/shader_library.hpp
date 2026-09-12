#pragma once

#include "shader_compile.hpp"
#include "shader_constants.hpp"

#include <d3d11.h>

namespace photorealism {

class ShaderLibrary {
  public:
    bool compile(ID3D11Device* device);
    void log_state() const;
    void release();

    ID3D11VertexShader* vertex() const { return vertex_shader_; }
    ID3D11PixelShader* visual() const { return pixel_shader_; }
    ID3D11PixelShader* depth_preview() const { return depth_preview_shader_; }
    ID3D11PixelShader* ssao() const { return ssao_shader_; }
    ID3D11PixelShader* temporal() const { return temporal_shader_; }
    ID3D11PixelShader* bloom_bright() const { return bloom_bright_shader_; }
    ID3D11PixelShader* bloom_downsample() const {
        return bloom_downsample_shader_;
    }
    ID3D11PixelShader* bloom_upsample() const {
        return bloom_upsample_shader_;
    }

  private:
    struct ShaderBlobs {
        ID3DBlob* vertex;
        ID3DBlob* pixel;
        ID3DBlob* depth_preview;
        ID3DBlob* ssao;
        ID3DBlob* temporal;
        ID3DBlob* bloom[kBloomPassCount];
    };

    struct CompiledShaders {
        ID3D11VertexShader* vertex;
        ID3D11PixelShader* pixel;
        ID3D11PixelShader* depth_preview;
        ID3D11PixelShader* ssao;
        ID3D11PixelShader* temporal;
        ID3D11PixelShader* bloom[kBloomPassCount];
    };

    ID3D11PixelShader* create_optional_pixel_shader(
        ID3D11Device* device, ID3DBlob* blob, const char* description);
    void adopt_optional_shader(
        ID3D11PixelShader** slot, ID3D11PixelShader* fresh);
    void release_shader_blobs(ShaderBlobs* blobs);
    bool compile_shader_blobs(
        CompileFromFileFunction compile_from_file, ShaderBlobs* blobs);
    void release_compiled_shaders(CompiledShaders* shaders);
    HRESULT create_core_shaders(
        ID3D11Device* device,
        const ShaderBlobs& blobs,
        CompiledShaders* shaders);
    void create_optional_shaders(
        ID3D11Device* device,
        const ShaderBlobs& blobs,
        CompiledShaders* shaders);
    void adopt_bloom_shaders(CompiledShaders* shaders);
    void adopt_compiled_shaders(CompiledShaders* shaders);

    ID3D11VertexShader* vertex_shader_ = nullptr;
    ID3D11PixelShader* pixel_shader_ = nullptr;
    ID3D11PixelShader* depth_preview_shader_ = nullptr;
    ID3D11PixelShader* ssao_shader_ = nullptr;
    ID3D11PixelShader* temporal_shader_ = nullptr;
    ID3D11PixelShader* bloom_bright_shader_ = nullptr;
    ID3D11PixelShader* bloom_downsample_shader_ = nullptr;
    ID3D11PixelShader* bloom_upsample_shader_ = nullptr;
};

}  // namespace photorealism
