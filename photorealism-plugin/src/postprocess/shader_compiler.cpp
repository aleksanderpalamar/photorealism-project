#include "shader_library.hpp"

#include "../runtime.hpp"
#include "com_utils.hpp"

#include <windows.h>

namespace photorealism {
namespace {
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


ID3DBlob* compile_shader_blob(
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

}  // namespace photorealism
