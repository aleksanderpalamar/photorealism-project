#include "output_target.hpp"

#include "../resource_observer/view_shape.hpp"
#include "../scene/formats.hpp"

namespace photorealism {
namespace fsr {

bool describe_output(ID3D11RenderTargetView* view, OutputTarget* target) {
    if (view == nullptr || target == nullptr) {
        return false;
    }
    ID3D11Texture2D* texture = nullptr;
    observer::TargetShape shape;
    if (!observer::describe_view(view, &texture, &shape)) {
        return false;
    }
    texture->Release();

    D3D11_RENDER_TARGET_VIEW_DESC description = {};
    view->GetDesc(&description);
    target->width = shape.width;
    target->height = shape.height;
    target->srgb_view = scene_formats::is_srgb(
        static_cast<unsigned>(description.Format));
    return true;
}

}
}
