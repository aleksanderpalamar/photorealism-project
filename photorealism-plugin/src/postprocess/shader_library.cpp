#include "shader_library.hpp"

#include "../runtime.hpp"
#include "com_utils.hpp"

#include <windows.h>

#include <cstdio>

namespace photorealism {
namespace {

CompileFromFileFunction resolve_shader_compiler() {
    static CompileFromFileFunction function = []() -> CompileFromFileFunction {
        HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
        if (compiler == nullptr) {
            log_message(
                "Nao foi possivel carregar d3dcompiler_47.dll: %lu.",
                GetLastError());
            return nullptr;
        }
        return reinterpret_cast<CompileFromFileFunction>(
            GetProcAddress(compiler, "D3DCompileFromFile"));
    }();
    return function;
}

void log_compile_error(const char* stage, HRESULT result, ID3DBlob* errors) {
    if (errors != nullptr && errors->GetBufferPointer() != nullptr) {
        log_message(
            "Erro no shader %s (0x%08X): %s",
            stage,
            static_cast<unsigned>(result),
            static_cast<const char*>(errors->GetBufferPointer()));
    } else {
        log_message(
            "Erro no shader %s: 0x%08X.",
            stage,
            static_cast<unsigned>(result));
    }
}

}  // namespace

ID3DBlob* ShaderLibrary::compile_shader_blob(
    CompileFromFileFunction compile_from_file,
    const wchar_t* path,
    const char* entry_point,
    const char* target,
    const char* stage) {
    constexpr UINT kFlags =
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    ID3DBlob* blob = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT result = compile_from_file(
        path,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry_point,
        target,
        kFlags,
        0,
        &blob,
        &errors);
    if (FAILED(result)) {
        log_compile_error(stage, result, errors);
        safe_release(blob);
    }
    safe_release(errors);
    return blob;
}

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

void ShaderLibrary::release_shader_blobs(ShaderLibrary::ShaderBlobs* blobs) {
    safe_release(blobs->vertex);
    safe_release(blobs->pixel);
    safe_release(blobs->depth_preview);
    safe_release(blobs->ssao);
    safe_release(blobs->temporal);
    for (UINT index = 0; index < kBloomPassCount; ++index) {
        safe_release(blobs->bloom[index]);
    }
}

bool ShaderLibrary::compile_shader_blobs(
    CompileFromFileFunction compile_from_file, ShaderLibrary::ShaderBlobs* blobs) {
    blobs->vertex = compile_shader_blob(
        compile_from_file, shader_path(), "VSMain", "vs_5_0", "vertex");
    if (blobs->vertex == nullptr) {
        return false;
    }
    blobs->pixel = compile_shader_blob(
        compile_from_file, shader_path(), "PSMain", "ps_5_0", "pixel");
    if (blobs->pixel == nullptr) {
        return false;
    }

    blobs->depth_preview = compile_shader_blob(
        compile_from_file,
        depth_preview_shader_path(),
        "PSDepthPreview",
        "ps_5_0",
        "depth preview");
    blobs->ssao = compile_shader_blob(
        compile_from_file, ssao_shader_path(), "PSSSAO", "ps_5_0", "SSAO");
    blobs->temporal = compile_shader_blob(
        compile_from_file,
        temporal_shader_path(),
        "PSTemporal",
        "ps_5_0",
        "temporal");
    for (UINT index = 0; index < kBloomPassCount; ++index) {
        blobs->bloom[index] = compile_shader_blob(
            compile_from_file,
            bloom_shader_path(),
            kBloomEntryPoints[index],
            "ps_5_0",
            kBloomEntryPoints[index]);
    }
    return true;
}

void ShaderLibrary::release_compiled_shaders(ShaderLibrary::CompiledShaders* shaders) {
    safe_release(shaders->vertex);
    safe_release(shaders->pixel);
    safe_release(shaders->depth_preview);
    safe_release(shaders->ssao);
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
    shaders->ssao = create_optional_pixel_shader(device, blobs.ssao, "SSAO");
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
    adopt_optional_shader(&ssao_shader_, shaders->ssao);
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
        "ssao_0.9.1=%s temporal_0.10.0=%s bloom_0.17.0=%s.",
        depth_preview_shader_ != nullptr ? "ok" : "indisponivel",
        ssao_shader_ != nullptr ? "ok" : "indisponivel",
        temporal_shader_ != nullptr ? "ok" : "indisponivel",
        bloom_bright_shader_ != nullptr ? "ok" : "indisponivel");
}

void ShaderLibrary::release() {
    safe_release(vertex_shader_);
    safe_release(pixel_shader_);
    safe_release(depth_preview_shader_);
    safe_release(ssao_shader_);
    safe_release(temporal_shader_);
    safe_release(bloom_bright_shader_);
    safe_release(bloom_downsample_shader_);
    safe_release(bloom_upsample_shader_);
}

}  // namespace photorealism
