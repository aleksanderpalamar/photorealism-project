#pragma once

#include <d3d11.h>

namespace photorealism {
namespace fsr {

struct OutputTarget {
    unsigned width = 0;
    unsigned height = 0;
    bool srgb_view = false;
};

bool describe_output(ID3D11RenderTargetView* view, OutputTarget* target);

}
}
