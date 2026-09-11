#pragma once

namespace photorealism {
namespace overlay {

constexpr unsigned kKeyUp = 1u;
constexpr unsigned kKeyDown = 2u;
constexpr unsigned kKeyLeft = 4u;
constexpr unsigned kKeyRight = 8u;
constexpr unsigned kKeyEnter = 16u;
constexpr unsigned kKeyTab = 32u;

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
