#pragma once

namespace photorealism {
namespace overlay {

struct PointerState {
    float x = 0.0f;
    float y = 0.0f;
    bool inside = false;
    bool down = false;
    bool pressed = false;
    bool released = false;
    float wheel = 0.0f;
};

}
}
