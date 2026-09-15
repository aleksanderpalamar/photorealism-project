#include "staging_snapshots.hpp"

#include "dds_header.hpp"

#include <cstdio>
#include <string>

namespace photorealism {
namespace frame_capture {
namespace {

std::string file_name(unsigned index, const ReleasedTarget& released) {
    char name[96] = {};
    std::snprintf(
        name, sizeof(name), "%03u_b%03u-%03u_id%02u_%s%u_%ux%u_f%u.dds", index,
        released.first_bind, released.last_bind, released.target.identity,
        released.target.depth ? "ds" : "rt", released.target.slot,
        released.target.width, released.target.height, released.target.texture_format);
    return name;
}

unsigned long long resource_bytes(const D3D11_TEXTURE2D_DESC& description) {
    unsigned long long total = 0ull;
    for (UINT mip = 0; mip < description.MipLevels; ++mip) {
        total += static_cast<unsigned long long>(description.Width >> mip) *
                 (description.Height >> mip) * bytes_per_pixel(description.Format);
    }
    return total * description.ArraySize;
}

bool write_subresource(
    ID3D11DeviceContext* context, ID3D11Texture2D* staging, unsigned subresource,
    const SnapshotRecord& record, const std::wstring& path) {
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(staging, subresource, D3D11_MAP_READ, 0, &mapped))) {
        return false;
    }
    FILE* file = _wfopen(path.c_str(), L"wb");
    const unsigned row_bytes =
        record.target.width * bytes_per_pixel(record.target.texture_format);
    const auto header = dds_header(
        record.target.width, record.target.height, record.target.texture_format);
    bool written = file != nullptr &&
                   std::fwrite(header.data(), 1, header.size(), file) == header.size();
    for (unsigned row = 0; written && row < record.target.height; ++row) {
        const auto* line = static_cast<const unsigned char*>(mapped.pData) +
                           static_cast<std::size_t>(row) * mapped.RowPitch;
        written = std::fwrite(line, 1, row_bytes, file) == row_bytes;
    }
    context->Unmap(staging, subresource);
    written = file != nullptr && std::fclose(file) == 0 && written;
    return written;
}

}

void StagingSnapshots::reset() {
    for (Pending& pending : pending_) {
        if (pending.staging != nullptr) {
            pending.staging->Release();
        }
    }
    pending_.clear();
    records_.clear();
    bytes_ = 0ull;
    truncated_ = false;
}

const char* StagingSnapshots::copy_to_staging(
    ID3D11DeviceContext* context, const TargetInfo& target, Pending* pending) {
    auto* texture = static_cast<ID3D11Texture2D*>(target.key.texture);
    D3D11_TEXTURE2D_DESC description = {};
    texture->GetDesc(&description);
    if (description.SampleDesc.Count != 1) {
        return "multiamostra";
    }
    if (bytes_per_pixel(description.Format) == 0) {
        return "formato sem tamanho conhecido";
    }
    const unsigned long long bytes = resource_bytes(description);
    if (records_.size() >= kMaximumSnapshots || bytes_ + bytes > kMaximumSnapshotBytes) {
        truncated_ = true;
        return "limite de memoria da captura";
    }
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;
    ID3D11Device* device = nullptr;
    context->GetDevice(&device);
    const HRESULT result = device->CreateTexture2D(&description, nullptr, &pending->staging);
    device->Release();
    if (FAILED(result)) {
        return "staging recusado pelo driver";
    }
    context->CopyResource(pending->staging, texture);
    pending->subresource = D3D11CalcSubresource(
        target.key.mip, target.key.slice, description.MipLevels);
    bytes_ += bytes;
    return nullptr;
}

void StagingSnapshots::take(ID3D11DeviceContext* context, const ReleasedTarget& released) {
    SnapshotRecord record;
    record.target = released.target;
    record.first_bind = released.first_bind;
    record.last_bind = released.last_bind;
    record.file = file_name(static_cast<unsigned>(records_.size()), released);
    Pending pending = {nullptr, 0};
    record.failure = copy_to_staging(context, released.target, &pending);
    records_.push_back(record);
    pending_.push_back(pending);
}

unsigned StagingSnapshots::write_all(ID3D11DeviceContext* context, const wchar_t* folder) {
    unsigned written = 0;
    for (std::size_t index = 0; index < records_.size(); ++index) {
        SnapshotRecord& record = records_[index];
        const Pending& pending = pending_[index];
        if (pending.staging == nullptr) {
            continue;
        }
        std::wstring path = folder;
        path.append(L"\\");
        path.append(record.file.begin(), record.file.end());
        const bool ok = write_subresource(context, pending.staging, pending.subresource, record, path);
        record.failure = ok ? nullptr : "falha ao gravar o arquivo";
        written += ok ? 1u : 0u;
    }
    return written;
}

}
}
