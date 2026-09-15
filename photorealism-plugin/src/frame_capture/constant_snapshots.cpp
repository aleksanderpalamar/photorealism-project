#include "constant_snapshots.hpp"

#include <cstdio>
#include <string>

namespace photorealism {
namespace frame_capture {
namespace {

constexpr UINT kSlots = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;

bool write_buffer(
    ID3D11DeviceContext* context, ID3D11Buffer* staging, const ConstantRecord& record,
    const std::wstring& path) {
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
        return false;
    }
    FILE* file = _wfopen(path.c_str(), L"wb");
    bool written = file != nullptr &&
                   std::fwrite(mapped.pData, 1, record.bytes, file) == record.bytes;
    context->Unmap(staging, 0);
    written = file != nullptr && std::fclose(file) == 0 && written;
    return written;
}

}

void ConstantSnapshots::reset() {
    for (ID3D11Buffer* buffer : staging_) {
        if (buffer != nullptr) {
            buffer->Release();
        }
    }
    staging_.clear();
    records_.clear();
}

const char* ConstantSnapshots::copy_buffer(
    ID3D11DeviceContext* context, ID3D11Buffer* buffer, unsigned* bytes,
    ID3D11Buffer** staging) {
    D3D11_BUFFER_DESC description = {};
    buffer->GetDesc(&description);
    *bytes = description.ByteWidth;
    if (description.ByteWidth > kMaximumConstantBytes) {
        return "buffer grande demais";
    }
    if (records_.size() >= kMaximumConstantSnapshots) {
        return "limite de constantes da captura";
    }
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;
    description.StructureByteStride = 0;
    ID3D11Device* device = nullptr;
    context->GetDevice(&device);
    const HRESULT result = device->CreateBuffer(&description, nullptr, staging);
    device->Release();
    if (FAILED(result)) {
        return "staging recusado pelo driver";
    }
    context->CopyResource(*staging, buffer);
    return nullptr;
}

void ConstantSnapshots::take_stage(
    ID3D11DeviceContext* context, unsigned bind, const char* stage,
    ID3D11Buffer* const* buffers) {
    for (UINT slot = 0; slot < kSlots; ++slot) {
        if (buffers[slot] == nullptr) {
            continue;
        }
        ConstantRecord record;
        record.bind = bind;
        record.stage = stage;
        record.slot = slot;
        char name[48] = {};
        std::snprintf(name, sizeof(name), "cb_b%03u_%s%u.bin", bind, stage, slot);
        record.file = name;
        ID3D11Buffer* staging = nullptr;
        record.failure = copy_buffer(context, buffers[slot], &record.bytes, &staging);
        records_.push_back(record);
        staging_.push_back(staging);
    }
}

void ConstantSnapshots::take(ID3D11DeviceContext* context, unsigned bind) {
    ID3D11Buffer* vertex[kSlots] = {};
    ID3D11Buffer* pixel[kSlots] = {};
    context->VSGetConstantBuffers(0, kSlots, vertex);
    context->PSGetConstantBuffers(0, kSlots, pixel);
    take_stage(context, bind, "vs", vertex);
    take_stage(context, bind, "ps", pixel);
    for (UINT slot = 0; slot < kSlots; ++slot) {
        if (vertex[slot] != nullptr) {
            vertex[slot]->Release();
        }
        if (pixel[slot] != nullptr) {
            pixel[slot]->Release();
        }
    }
}

unsigned ConstantSnapshots::write_all(ID3D11DeviceContext* context, const wchar_t* folder) {
    unsigned written = 0;
    for (std::size_t index = 0; index < records_.size(); ++index) {
        ConstantRecord& record = records_[index];
        if (staging_[index] == nullptr) {
            continue;
        }
        std::wstring path = folder;
        path.append(L"\\");
        path.append(record.file.begin(), record.file.end());
        const bool ok = write_buffer(context, staging_[index], record, path);
        record.failure = ok ? nullptr : "falha ao gravar o arquivo";
        written += ok ? 1u : 0u;
    }
    return written;
}

}
}
