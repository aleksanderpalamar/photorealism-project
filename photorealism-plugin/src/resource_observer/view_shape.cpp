#include "view_shape.hpp"

namespace photorealism {
namespace observer {

bool describe_view(ID3D11View* view, ID3D11Texture2D** texture, TargetShape* shape) {
    if (view == nullptr || texture == nullptr || shape == nullptr) {
        return false;
    }
    ID3D11Resource* resource = nullptr;
    view->GetResource(&resource);
    if (resource == nullptr) {
        return false;
    }
    resource->QueryInterface(
        IID_ID3D11Texture2D, reinterpret_cast<void**>(texture));
    resource->Release();
    if (*texture == nullptr) {
        return false;
    }
    D3D11_TEXTURE2D_DESC described = {};
    (*texture)->GetDesc(&described);
    shape->width = described.Width;
    shape->height = described.Height;
    shape->format = static_cast<unsigned>(described.Format);
    shape->samples = described.SampleDesc.Count;
    return true;
}

void release_texture_handle(void* handle) {
    if (handle == nullptr) {
        return;
    }
    static_cast<ID3D11Texture2D*>(handle)->Release();
}

}
}
