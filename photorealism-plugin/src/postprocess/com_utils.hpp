#pragma once

namespace photorealism {

template <typename T>
void safe_release(T*& object) {
    if (object == nullptr) {
        return;
    }
    object->Release();
    object = nullptr;
}

}  // namespace photorealism
