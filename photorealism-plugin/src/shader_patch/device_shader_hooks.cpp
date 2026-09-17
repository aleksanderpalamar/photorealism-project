#include "device_shader_hooks.hpp"

#include <atomic>
#include <vector>

#include "../hooks/vtable_patch.hpp"
#include "../runtime.hpp"
#include "shader_patch.hpp"

namespace photorealism {
namespace shader_patch {
namespace {

constexpr unsigned kCreatePixelShaderSlot = 15;

std::atomic<CreatePixelShaderFunction> g_original_create_pixel_shader{nullptr};
thread_local unsigned g_dispatch_depth = 0;

class DispatchScope {
  public:
    DispatchScope() : outermost_(g_dispatch_depth++ == 0) {}
    ~DispatchScope() { --g_dispatch_depth; }

    bool outermost() const { return outermost_; }

  private:
    bool outermost_;
};

}  // namespace

HRESULT STDMETHODCALLTYPE hooked_create_pixel_shader(
    ID3D11Device* device,
    const void* bytecode,
    SIZE_T length,
    ID3D11ClassLinkage* linkage,
    ID3D11PixelShader** shader) {
    CreatePixelShaderFunction original =
        g_original_create_pixel_shader.load(std::memory_order_acquire);
    if (original == nullptr) {
        return E_FAIL;
    }

    DispatchScope scope;
    if (!scope.outermost() || !is_enabled()) {
        return original(device, bytecode, length, linkage, shader);
    }

    std::vector<std::uint8_t> patched;
    if (patch_pixel_shader(bytecode, static_cast<std::size_t>(length), &patched)) {
        const HRESULT result = original(
            device, patched.data(), patched.size(), linkage, shader);
        if (SUCCEEDED(result)) {
            return result;
        }
        log_message(
            "Patch de shader: o shader trocado nao foi aceito (0x%08X); "
            "voltando ao original e descartando o cache.",
            static_cast<unsigned>(result));
        discard_patched_shader(bytecode, static_cast<std::size_t>(length));
    }
    return original(device, bytecode, length, linkage, shader);
}

bool install_device_shader_hooks(ID3D11Device* device) {
    if (device == nullptr) {
        return false;
    }
    if (g_original_create_pixel_shader.load(std::memory_order_acquire) !=
        nullptr) {
        return true;
    }
    void** vtable = *reinterpret_cast<void***>(device);
    return replace_vtable_entry(
        &vtable[kCreatePixelShaderSlot],
        reinterpret_cast<void*>(&hooked_create_pixel_shader),
        &g_original_create_pixel_shader);
}

}
}
