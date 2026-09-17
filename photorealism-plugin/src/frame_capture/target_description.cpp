#include "target_description.hpp"

namespace photorealism {
namespace frame_capture {
namespace {

struct ViewPlacement {
    unsigned mip;
    unsigned slice;
    unsigned format;
};

ViewPlacement placement_of(ID3D11RenderTargetView* view) {
    D3D11_RENDER_TARGET_VIEW_DESC description = {};
    view->GetDesc(&description);
    const bool array = description.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    return {array ? description.Texture2DArray.MipSlice : description.Texture2D.MipSlice,
            array ? description.Texture2DArray.FirstArraySlice : 0u,
            static_cast<unsigned>(description.Format)};
}

ViewPlacement placement_of(ID3D11DepthStencilView* view) {
    D3D11_DEPTH_STENCIL_VIEW_DESC description = {};
    view->GetDesc(&description);
    const bool array = description.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    return {array ? description.Texture2DArray.MipSlice : description.Texture2D.MipSlice,
            array ? description.Texture2DArray.FirstArraySlice : 0u,
            static_cast<unsigned>(description.Format)};
}

template <typename View>
bool describe_view(View* view, unsigned slot, bool depth, TargetInfo* target) {
    if (view == nullptr) {
        return false;
    }
    ID3D11Resource* resource = nullptr;
    view->GetResource(&resource);
    ID3D11Texture2D* texture = nullptr;
    if (resource != nullptr) {
        resource->QueryInterface(IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture));
        resource->Release();
    }
    if (texture == nullptr) {
        return false;
    }
    D3D11_TEXTURE2D_DESC description = {};
    texture->GetDesc(&description);
    const ViewPlacement placement = placement_of(view);
    target->key = {texture, placement.mip, placement.slice};
    target->slot = slot;
    target->depth = depth;
    target->width = description.Width >> placement.mip;
    target->height = description.Height >> placement.mip;
    target->texture_format = static_cast<unsigned>(description.Format);
    target->view_format = placement.format;
    target->mip_levels = description.MipLevels;
    target->array_size = description.ArraySize;
    target->samples = description.SampleDesc.Count;
    texture->Release();
    return true;
}

}

std::vector<TargetInfo> describe_binding(
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    ID3D11DepthStencilView* depth_target) {
    std::vector<TargetInfo> targets;
    const UINT count = render_targets != nullptr ? render_target_count : 0;
    for (UINT slot = 0; slot < count && slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++slot) {
        TargetInfo target;
        if (describe_view(render_targets[slot], slot, false, &target)) {
            targets.push_back(target);
        }
    }
    TargetInfo depth;
    if (describe_view(depth_target, 0u, true, &depth)) {
        targets.push_back(depth);
    }
    return targets;
}

}
}
