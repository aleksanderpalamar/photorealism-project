#include "renderer.hpp"

#include "../postprocess/com_utils.hpp"
#include "../runtime.hpp"

namespace photorealism {
namespace overlay {

bool Renderer::create_font_texture(
    ID3D11Device* device, const BakedFont& font) {
    if (font.pixels.empty() || font.width <= 0 || font.height <= 0) {
        log_message("Overlay recebeu um atlas de fonte vazio.");
        return false;
    }

    D3D11_TEXTURE2D_DESC description = {};
    description.Width = static_cast<UINT>(font.width);
    description.Height = static_cast<UINT>(font.height);
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = DXGI_FORMAT_R8_UNORM;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initial = {};
    initial.pSysMem = font.pixels.data();
    initial.SysMemPitch = static_cast<UINT>(font.width);

    const HRESULT result =
        device->CreateTexture2D(&description, &initial, &font_texture_);
    if (FAILED(result)) {
        log_message(
            "Overlay falhou ao criar a textura da fonte: 0x%08X.",
            static_cast<unsigned>(result));
        return false;
    }
    return SUCCEEDED(
        device->CreateShaderResourceView(font_texture_, nullptr, &font_view_));
}

bool Renderer::ensure_capacity(std::size_t vertex_count) {
    if (vertices_ != nullptr && capacity_ >= vertex_count) {
        return true;
    }
    if (device_ == nullptr) {
        return false;
    }
    safe_release(vertex_view_);
    safe_release(vertices_);
    capacity_ = 0;

    std::size_t capacity = kInitialVertexCapacity;
    while (capacity < vertex_count) {
        capacity *= 2;
    }

    D3D11_BUFFER_DESC description = {};
    description.ByteWidth = static_cast<UINT>(capacity * sizeof(Vertex));
    description.Usage = D3D11_USAGE_DYNAMIC;
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    description.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    description.StructureByteStride = sizeof(Vertex);
    const HRESULT created =
        device_->CreateBuffer(&description, nullptr, &vertices_);
    if (FAILED(created)) {
        log_message(
            "Overlay falhou ao criar o buffer de vertices: 0x%08X.",
            static_cast<unsigned>(created));
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC view = {};
    view.Format = DXGI_FORMAT_UNKNOWN;
    view.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    view.Buffer.FirstElement = 0;
    view.Buffer.NumElements = static_cast<UINT>(capacity);
    if (FAILED(device_->CreateShaderResourceView(
            vertices_, &view, &vertex_view_))) {
        log_message("Overlay falhou ao criar a view do buffer de vertices.");
        safe_release(vertices_);
        return false;
    }
    capacity_ = capacity;
    return true;
}

}
}
