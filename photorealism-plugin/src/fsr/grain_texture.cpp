#include "grain_texture.hpp"

#include "../postprocess/com_utils.hpp"
#include "../runtime.hpp"
#include "blue_noise.hpp"
#include "rcas_constants.hpp"

#include <vector>
#include <windows.h>

namespace photorealism {
namespace fsr {

bool GrainTexture::create(ID3D11Device* device) {
    release();
    if (device == nullptr) {
        return false;
    }

    const ULONGLONG started = GetTickCount64();
    const std::vector<float> noise = generate_blue_noise(kGrainTileSize);
    const ULONGLONG elapsed = GetTickCount64() - started;

    D3D11_TEXTURE2D_DESC description = {};
    description.Width = kGrainTileSize;
    description.Height = kGrainTileSize;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R32_FLOAT;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = noise.data();
    data.SysMemPitch = kGrainTileSize * sizeof(float);

    HRESULT result = device->CreateTexture2D(&description, &data, &texture_);
    if (SUCCEEDED(result)) {
        result = device->CreateShaderResourceView(texture_, nullptr, &view_);
    }
    if (FAILED(result)) {
        log_message(
            "FSR nao criou a textura de ruido azul do LFGA: 0x%08X.",
            static_cast<unsigned>(result));
        release();
        return false;
    }
    log_message(
        "FSR gerou ruido azul %ux%u para a granulacao LFGA em %llu ms.",
        kGrainTileSize,
        kGrainTileSize,
        static_cast<unsigned long long>(elapsed));
    return true;
}

void GrainTexture::release() {
    safe_release(view_);
    safe_release(texture_);
}

}
}
