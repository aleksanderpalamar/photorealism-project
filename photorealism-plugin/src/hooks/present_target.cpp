#include "present_target.hpp"

namespace photorealism {

bool present_back_buffer(
    IDXGISwapChain* swap_chain, ID3D11Texture2D** texture) {
    if (swap_chain == nullptr || texture == nullptr) {
        return false;
    }
    const HRESULT result = swap_chain->GetBuffer(
        0, IID_ID3D11Texture2D, reinterpret_cast<void**>(texture));
    return SUCCEEDED(result) && *texture != nullptr;
}

}
