#include "shader_library.hpp"

#include "../runtime.hpp"
#include "com_utils.hpp"

#include <windows.h>

#include <cstdio>

namespace photorealism {


ID3D11PixelShader* ShaderLibrary::create_optional_pixel_shader(
    ID3D11Device* device, ID3DBlob* blob, const char* description) {
    if (blob == nullptr) {
        return nullptr;
    }
    ID3D11PixelShader* shader = nullptr;
    const HRESULT result = device->CreatePixelShader(
        blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader);
    if (SUCCEEDED(result)) {
        return shader;
    }
    log_message(
        "Falha ao criar shader %s: 0x%08X.",
        description,
        static_cast<unsigned>(result));
    safe_release(shader);
    return nullptr;
}

void ShaderLibrary::adopt_optional_shader(
    ID3D11PixelShader** slot, ID3D11PixelShader* fresh) {
    if (fresh == nullptr) {
        return;
    }
    safe_release(*slot);
    *slot = fresh;
}



void ShaderLibrary::release_compiled_shaders(ShaderLibrary::CompiledShaders* shaders) {
    safe_release(shaders->vertex);
    safe_release(shaders->pixel);
    safe_release(shaders->depth_preview);
    safe_release(shaders->temporal);
    for (UINT index = 0; index < kBloomPassCount; ++index) {
        safe_release(shaders->bloom[index]);
    }
}

HRESULT ShaderLibrary::create_core_shaders(
    ID3D11Device* device,
    const ShaderLibrary::ShaderBlobs& blobs,
    ShaderLibrary::CompiledShaders* shaders) {
    HRESULT result = device->CreateVertexShader(
        blobs.vertex->GetBufferPointer(),
        blobs.vertex->GetBufferSize(),
        nullptr,
        &shaders->vertex);
    if (FAILED(result)) {
        return result;
    }
    return device->CreatePixelShader(
        blobs.pixel->GetBufferPointer(),
        blobs.pixel->GetBufferSize(),
        nullptr,
        &shaders->pixel);
}

void ShaderLibrary::create_optional_shaders(
    ID3D11Device* device,
    const ShaderLibrary::ShaderBlobs& blobs,
    ShaderLibrary::CompiledShaders* shaders) {
    shaders->depth_preview =
        create_optional_pixel_shader(device, blobs.depth_preview, "de preview depth");
    shaders->temporal =
        create_optional_pixel_shader(device, blobs.temporal, "temporal");
    for (UINT index = 0; index < kBloomPassCount; ++index) {
        char description[64] = {};
        std::snprintf(
            description,
            sizeof(description),
            "%s do bloom",
            kBloomEntryPoints[index]);
        shaders->bloom[index] =
            create_optional_pixel_shader(device, blobs.bloom[index], description);
    }
}

void ShaderLibrary::adopt_bloom_shaders(ShaderLibrary::CompiledShaders* shaders) {
    bool complete = true;
    for (UINT index = 0; index < kBloomPassCount; ++index) {
        complete = complete && shaders->bloom[index] != nullptr;
    }
    if (!complete) {
        for (UINT index = 0; index < kBloomPassCount; ++index) {
            safe_release(shaders->bloom[index]);
        }
        return;
    }
    safe_release(bloom_bright_shader_);
    safe_release(bloom_downsample_shader_);
    safe_release(bloom_upsample_shader_);
    bloom_bright_shader_ = shaders->bloom[0];
    bloom_downsample_shader_ = shaders->bloom[1];
    bloom_upsample_shader_ = shaders->bloom[2];
}

void ShaderLibrary::adopt_compiled_shaders(ShaderLibrary::CompiledShaders* shaders) {
    safe_release(vertex_shader_);
    safe_release(pixel_shader_);
    vertex_shader_ = shaders->vertex;
    pixel_shader_ = shaders->pixel;
    adopt_optional_shader(&depth_preview_shader_, shaders->depth_preview);
    adopt_optional_shader(&temporal_shader_, shaders->temporal);
    adopt_bloom_shaders(shaders);
}

bool ShaderLibrary::compile(ID3D11Device* device) {
    const CompileFromFileFunction compile_from_file =
        resolve_shader_compiler();
    if (compile_from_file == nullptr) {
        log_message("D3DCompileFromFile nao esta disponivel.");
        return false;
    }

    ShaderLibrary::ShaderBlobs blobs = {};
    if (!compile_shader_blobs(compile_from_file, &blobs)) {
        release_shader_blobs(&blobs);
        return false;
    }

    ShaderLibrary::CompiledShaders shaders = {};
    const HRESULT result = create_core_shaders(device, blobs, &shaders);
    create_optional_shaders(device, blobs, &shaders);
    release_shader_blobs(&blobs);

    if (FAILED(result)) {
        log_message(
            "Falha ao criar shaders D3D11: 0x%08X.",
            static_cast<unsigned>(result));
        release_compiled_shaders(&shaders);
        return false;
    }

    adopt_compiled_shaders(&shaders);
    return true;
}

void ShaderLibrary::log_state() const {
    log_message(
        "Shaders Photorealism compilados: visual=ok depth_preview=%s "
        "temporal_0.10.0=%s bloom_0.17.0=%s.",
        depth_preview_shader_ != nullptr ? "ok" : "indisponivel",
        temporal_shader_ != nullptr ? "ok" : "indisponivel",
        bloom_bright_shader_ != nullptr ? "ok" : "indisponivel");
}

void ShaderLibrary::release() {
    safe_release(vertex_shader_);
    safe_release(pixel_shader_);
    safe_release(depth_preview_shader_);
    safe_release(temporal_shader_);
    safe_release(bloom_bright_shader_);
    safe_release(bloom_downsample_shader_);
    safe_release(bloom_upsample_shader_);
}

}  // namespace photorealism
