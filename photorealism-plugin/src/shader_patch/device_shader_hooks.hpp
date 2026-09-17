#pragma once

#include <d3d11.h>

namespace photorealism {
namespace shader_patch {

using CreatePixelShaderFunction = HRESULT(STDMETHODCALLTYPE*)(
    ID3D11Device*,
    const void*,
    SIZE_T,
    ID3D11ClassLinkage*,
    ID3D11PixelShader**);

bool install_device_shader_hooks(ID3D11Device* device);

HRESULT STDMETHODCALLTYPE hooked_create_pixel_shader(
    ID3D11Device* device,
    const void* bytecode,
    SIZE_T length,
    ID3D11ClassLinkage* linkage,
    ID3D11PixelShader** shader);

}
}
