#include "effect_shaders.hpp"

#include "../runtime.hpp"
#include "com_utils.hpp"
#include "shader_compile.hpp"

#include <cwchar>

namespace photorealism {
namespace {

struct EffectShaderSource {
    const wchar_t* file;
    const char* entry_point;
};

constexpr EffectShaderSource kSources[kEffectShaderCount] = {
    {L"fxaa.hlsl", "PSFxaa"},
    {L"ssao_occlusion.hlsl", "PSAmbientOcclusion"},
    {L"ssao_compose.hlsl", "PSComposeOcclusion"},
    {L"interior_light.hlsl", "PSInteriorLight"},
    {L"fsr_rcas.hlsl", "PSRcas"},
};

void shader_file_path(const wchar_t* file, wchar_t* path, std::size_t size) {
    std::swprintf(path, size, L"%ls\\shaders\\%ls", plugin_root(), file);
}

ID3D11PixelShader* create_shader(
    ID3D11Device* device, CompileFromFileFunction compile,
    const EffectShaderSource& source) {
    wchar_t path[MAX_PATH] = {};
    shader_file_path(source.file, path, MAX_PATH);
    ID3DBlob* blob = compile_shader_blob(
        compile, path, source.entry_point, "ps_5_0", source.entry_point);
    if (blob == nullptr) {
        return nullptr;
    }
    ID3D11PixelShader* shader = nullptr;
    const HRESULT result = device->CreatePixelShader(
        blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader);
    safe_release(blob);
    return SUCCEEDED(result) ? shader : nullptr;
}

}

void EffectShaders::compile(ID3D11Device* device) {
    release();
    const CompileFromFileFunction compile = resolve_shader_compiler();
    if (device == nullptr || compile == nullptr) {
        return;
    }
    for (unsigned index = 0; index < kEffectShaderCount; ++index) {
        shaders_[index] = create_shader(device, compile, kSources[index]);
    }
    log_message(
        "Shaders de efeito 0.23.3: fxaa=%s ssao_oclusao=%s ssao_composicao=%s "
        "luz_interior=%s nitidez_aa=%s.",
        available(EffectShader::Fxaa) ? "ok" : "falhou",
        available(EffectShader::Occlusion) ? "ok" : "falhou",
        available(EffectShader::Compose) ? "ok" : "falhou",
        available(EffectShader::InteriorLight) ? "ok" : "falhou",
        available(EffectShader::Sharpen) ? "ok" : "falhou");
}

void EffectShaders::release() {
    for (ID3D11PixelShader*& shader : shaders_) {
        safe_release(shader);
    }
}

}
