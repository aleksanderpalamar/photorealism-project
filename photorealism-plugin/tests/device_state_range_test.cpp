#include "../src/postprocess/device_state.hpp"

#include <d3d11_1.h>

#include <cstdio>

int main() {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    const HRESULT created = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &device, nullptr, &context);
    if (FAILED(created)) {
        std::printf("device_state_range_test unavailable: 0x%08X\n", static_cast<unsigned>(created));
        return 77;
    }

    ID3D11DeviceContext1* ranged = nullptr;
    const HRESULT queried = context->QueryInterface(
        IID_ID3D11DeviceContext1, reinterpret_cast<void**>(&ranged));
    if (FAILED(queried) || ranged == nullptr) {
        context->Release();
        device->Release();
        std::puts("device_state_range_test unavailable: D3D11.1 context");
        return 77;
    }

    D3D11_BUFFER_DESC description = {};
    description.ByteWidth = 1024;
    description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ID3D11Buffer* buffer = nullptr;
    const HRESULT buffer_result = device->CreateBuffer(&description, nullptr, &buffer);
    if (FAILED(buffer_result)) {
        ranged->Release();
        context->Release();
        device->Release();
        return 1;
    }

    UINT first = 16;
    UINT count = 16;
    ranged->PSSetConstantBuffers1(0, 1, &buffer, &first, &count);
    photorealism::SavedState state = {};
    photorealism::capture_state(context, &state);
    first = 0;
    ranged->PSSetConstantBuffers1(0, 1, &buffer, &first, &count);
    photorealism::restore_state(context, &state);

    ID3D11Buffer* restored = nullptr;
    ranged->PSGetConstantBuffers1(0, 1, &restored, &first, &count);
    const bool passed = restored == buffer && first == 16 && count == 16;
    if (restored != nullptr) {
        restored->Release();
    }
    buffer->Release();
    ranged->Release();
    context->Release();
    device->Release();
    std::puts(passed ? "device_state_range_test ok" : "device_state_range_test failed");
    return passed ? 0 : 1;
}
