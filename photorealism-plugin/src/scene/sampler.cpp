#include "sampler.hpp"

#include "../runtime.hpp"
#include "formats.hpp"

namespace photorealism {



void SceneSampler::drain(
    ID3D11DeviceContext* context, SceneSampleSink* sink) {
    for (Slot& slot : slots_) {
        if (!slot.pending) {
            continue;
        }

        if (context->GetData(
                slot.completion,
                nullptr,
                0u,
                D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
            continue;
        }
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(context->Map(
                slot.staging, 0u, D3D11_MAP_READ, 0u, &mapped)) &&
            mapped.pData != nullptr) {
            sink->on_sample(
                static_cast<const unsigned char*>(mapped.pData),
                mip_width_,
                mip_height_,
                mapped.RowPitch,
                scene_formats::is_bgra(static_cast<unsigned>(sample_format_)));
            context->Unmap(slot.staging, 0u);
        }
        slot.pending = false;
    }
}

bool SceneSampler::submit(
    ID3D11DeviceContext* context, ID3D11Texture2D* scene) {
    Slot& slot = slots_[next_slot_];
    next_slot_ = (next_slot_ + 1u) % 2u;
    if (slot.pending) {
        return false;
    }

    context->CopySubresourceRegion(
        pyramid_, 0u, 0u, 0u, 0u, scene, 0u, nullptr);
    context->GenerateMips(pyramid_view_);
    context->CopySubresourceRegion(
        slot.staging, 0u, 0u, 0u, 0u, pyramid_, mip_level_, nullptr);
    context->End(slot.completion);
    slot.pending = true;
    return true;
}

SceneSampler::Info SceneSampler::info() const {
    Info info = {};
    info.source_width = source_width_;
    info.source_height = source_height_;
    info.source_format = static_cast<unsigned>(source_format_);
    info.sample_format = static_cast<unsigned>(sample_format_);
    info.mip_level = mip_level_;
    info.mip_width = mip_width_;
    info.mip_height = mip_height_;
    return info;
}

bool SceneSampler::consume_creation_notice() {
    const bool notice = creation_notice_;
    creation_notice_ = false;
    return notice;
}

}
