#include "depth_capture.hpp"

#include "../runtime.hpp"
#include "com_utils.hpp"
#include "format_utils.hpp"

namespace photorealism {
namespace {

void release_pair(
    ID3D11ShaderResourceView** view, ID3D11Texture2D** texture) {
    safe_release(*view);
    safe_release(*texture);
}

}  // namespace

void DepthCapture::attach(
    ID3D11Device* device, ID3D11DeviceContext* context) {
    device_ = device;
    context_ = context;
}

void DepthCapture::release() {
    release_pair(&view_, &texture_);
    candidate_generation_ = 0;
    failed_generation_ = 0;
}

void DepthCapture::copy_from(ID3D11Texture2D* source) {
    context_->CopyResource(texture_, source);
}

bool DepthCapture::ensure(
    ID3D11Texture2D* source,
    const D3D11_TEXTURE2D_DESC& source_description,
    std::uint64_t generation,
    bool shader_available) {
    if (source == nullptr || device_ == nullptr || !shader_available ||
        generation == 0) {
        return false;
    }
    if (texture_ != nullptr && view_ != nullptr &&
        candidate_generation_ == generation) {
        return true;
    }
    if (failed_generation_ == generation) {
        return false;
    }

    release();

    ID3D11Device* source_device = nullptr;
    source->GetDevice(&source_device);
    const bool same_device = source_device == device_;
    safe_release(source_device);
    if (!same_device) {
        log_message(
            "Candidato depth pertence a outro dispositivo; "
            "captura recusada.");
        failed_generation_ = generation;
        return false;
    }

    DXGI_FORMAT resource_format = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT view_format = DXGI_FORMAT_UNKNOWN;
    if (source_description.SampleDesc.Count != 1 ||
        source_description.ArraySize != 1 ||
        !depth_copy_formats(
            source_description.Format, &resource_format, &view_format)) {
        log_message(
            "Candidato depth incompativel com copia 0.9.1: "
            "size=%ux%u format=%u samples=%u array=%u.",
            source_description.Width,
            source_description.Height,
            static_cast<unsigned>(source_description.Format),
            source_description.SampleDesc.Count,
            source_description.ArraySize);
        failed_generation_ = generation;
        return false;
    }

    D3D11_TEXTURE2D_DESC copy_description = source_description;
    copy_description.Format = resource_format;
    copy_description.Usage = D3D11_USAGE_DEFAULT;
    copy_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    copy_description.CPUAccessFlags = 0;
    copy_description.MiscFlags = 0;
    HRESULT result = device_->CreateTexture2D(
        &copy_description, nullptr, &texture_);
    if (SUCCEEDED(result) && texture_ != nullptr) {
        D3D11_SHADER_RESOURCE_VIEW_DESC view_description = {};
        view_description.Format = view_format;
        view_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        view_description.Texture2D.MostDetailedMip = 0;
        view_description.Texture2D.MipLevels = 1;
        result = device_->CreateShaderResourceView(
            texture_, &view_description, &view_);
    }
    if (FAILED(result) || texture_ == nullptr ||
        view_ == nullptr) {
        log_message(
            "Falha ao criar copia depth legivel: result=0x%08X "
            "source_format=%u resource_format=%u view_format=%u.",
            static_cast<unsigned>(result),
            static_cast<unsigned>(source_description.Format),
            static_cast<unsigned>(resource_format),
            static_cast<unsigned>(view_format));
        safe_release(view_);
        safe_release(texture_);
        failed_generation_ = generation;
        return false;
    }

    candidate_generation_ = generation;
    failed_generation_ = 0;
    log_message(
        "Recursos de copia depth criados: generation=%llu size=%ux%u "
        "source_format=%u resource_format=%u view_format=%u.",
        static_cast<unsigned long long>(generation),
        source_description.Width,
        source_description.Height,
        static_cast<unsigned>(source_description.Format),
        static_cast<unsigned>(resource_format),
        static_cast<unsigned>(view_format));
    return true;
}

}  // namespace photorealism
