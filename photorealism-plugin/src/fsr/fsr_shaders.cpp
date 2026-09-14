#include "fsr_shaders.hpp"

#include "../postprocess/com_utils.hpp"
#include "../postprocess/shader_compile.hpp"
#include "../runtime.hpp"
#include "upscale_resources.hpp"

namespace photorealism {
namespace fsr {
namespace {

ID3DBlob* compile_stage(
    CompileFromFileFunction compile,
    const wchar_t* path,
    const char* entry_point,
    const char* target) {
    return compile_shader_blob(compile, path, entry_point, target, entry_point);
}

bool create_constant_buffer(
    ID3D11Device* device, UINT size, ID3D11Buffer** buffer) {
    D3D11_BUFFER_DESC description = {};
    description.ByteWidth = size;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    return SUCCEEDED(device->CreateBuffer(&description, nullptr, buffer));
}

}

bool FsrShaders::create_shaders(ID3D11Device* device) {
    CompileFromFileFunction compile = resolve_shader_compiler();
    if (compile == nullptr) {
        log_message("FSR sem compilador de shader disponivel.");
        return false;
    }

    ID3DBlob* easu_blob =
        compile_stage(compile, fsr_easu_shader_path(), "CSEasu", "cs_5_0");
    if (easu_blob == nullptr) {
        return false;
    }
    const HRESULT easu_created = device->CreateComputeShader(
        easu_blob->GetBufferPointer(), easu_blob->GetBufferSize(), nullptr,
        &easu_);
    easu_blob->Release();
    if (FAILED(easu_created)) {
        log_message("FSR nao criou o compute shader do EASU.");
        return false;
    }

    ID3DBlob* vertex_blob =
        compile_stage(compile, fsr_rcas_shader_path(), "VSRcas", "vs_5_0");
    if (vertex_blob == nullptr) {
        return false;
    }
    const HRESULT vertex_created = device->CreateVertexShader(
        vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), nullptr,
        &rcas_vertex_);
    vertex_blob->Release();
    if (FAILED(vertex_created)) {
        return false;
    }

    ID3DBlob* pixel_blob =
        compile_stage(compile, fsr_rcas_shader_path(), "PSRcas", "ps_5_0");
    if (pixel_blob == nullptr) {
        return false;
    }
    const HRESULT pixel_created = device->CreatePixelShader(
        pixel_blob->GetBufferPointer(), pixel_blob->GetBufferSize(), nullptr,
        &rcas_pixel_);
    pixel_blob->Release();
    return SUCCEEDED(pixel_created);
}

bool FsrShaders::create_buffers(ID3D11Device* device) {
    if (!create_constant_buffer(
            device, sizeof(EasuConstants), &easu_constants_)) {
        return false;
    }
    if (!create_constant_buffer(
            device, sizeof(RcasConstants), &rcas_constants_)) {
        return false;
    }

    D3D11_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    return SUCCEEDED(device->CreateSamplerState(&sampler, &sampler_));
}

bool FsrShaders::create(ID3D11Device* device) {
    release();
    if (device == nullptr) {
        return false;
    }
    if (!create_shaders(device) || !create_buffers(device)) {
        release();
        return false;
    }
    log_message("FSR compilou EASU em compute e RCAS em pixel shader.");
    return true;
}

void FsrShaders::release() {
    safe_release(sampler_);
    safe_release(rcas_constants_);
    safe_release(easu_constants_);
    safe_release(rcas_pixel_);
    safe_release(rcas_vertex_);
    safe_release(easu_);
}

}
}
