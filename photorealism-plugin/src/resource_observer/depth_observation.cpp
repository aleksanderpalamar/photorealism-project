#include "resource_observer.hpp"

#include "../runtime.hpp"
#include "discovery.hpp"
#include "observer_lock.hpp"
#include "observer_state.hpp"

#include <windows.h>

namespace photorealism {

using namespace observer;

void observe_depth_activity(
    ID3D11DeviceContext* context,
    ID3D11DepthStencilView* depth_target) {
    if (context == nullptr || depth_target == nullptr ||
        context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
        return;
    }

    AcquireSRWLockShared(&g_observer_lock);
    if (!g_depth_candidate_finalized) {
        ReleaseSRWLockShared(&g_observer_lock);
        return;
    }
    ViewCacheEntry* cached = find_cached_view(depth_target);
    if (cached != nullptr &&
        cached->resource_index < kMaximumObservedResources) {
        ObservedDepthResource& resource =
            g_observed_resources[cached->resource_index];
        if (resource.identity != nullptr &&
            resource.generation == cached->resource_generation &&
            resource.identity == g_retained_candidate_identity) {
            g_depth_candidate_binding_serial.fetch_add(
                1, std::memory_order_release);
            ReleaseSRWLockShared(&g_observer_lock);
            return;
        }
    }
    ReleaseSRWLockShared(&g_observer_lock);

    ID3D11Resource* resource_interface = nullptr;
    ID3D11Texture2D* texture = nullptr;
    depth_target->GetResource(&resource_interface);
    if (resource_interface == nullptr || FAILED(resource_interface->QueryInterface(
            IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture))) ||
        texture == nullptr) {
        if (resource_interface != nullptr) {
            resource_interface->Release();
        }
        return;
    }

    AcquireSRWLockShared(&g_observer_lock);
    if (g_depth_candidate_finalized &&
        static_cast<void*>(texture) == g_retained_candidate_identity) {
        g_depth_candidate_binding_serial.fetch_add(
            1, std::memory_order_release);
    }
    ReleaseSRWLockShared(&g_observer_lock);
    texture->Release();
    resource_interface->Release();
}

void observe_depth_target(
    ID3D11DeviceContext* context,
    ID3D11DepthStencilView* depth_target) {
    if (context == nullptr || depth_target == nullptr ||
        context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
        return;
    }
    if (!g_discovery_active.load(std::memory_order_acquire)) {
        observe_depth_activity(context, depth_target);
        return;
    }

    AcquireSRWLockShared(&g_observer_lock);
    if (!g_discovery_active.load(std::memory_order_relaxed)) {
        ReleaseSRWLockShared(&g_observer_lock);
        observe_depth_activity(context, depth_target);
        return;
    }
    ViewCacheEntry* cached = find_cached_view(depth_target);
    if (cached != nullptr &&
        cached->resource_index < kMaximumObservedResources) {
        ObservedDepthResource& resource =
            g_observed_resources[cached->resource_index];
        if (resource.identity != nullptr &&
            resource.generation == cached->resource_generation) {
            InterlockedIncrement64(&resource.bindings);
            ReleaseSRWLockShared(&g_observer_lock);
            finish_discovery_if_due();
            return;
        }
    }
    ReleaseSRWLockShared(&g_observer_lock);

    D3D11_DEPTH_STENCIL_VIEW_DESC view_description = {};
    depth_target->GetDesc(&view_description);

    ID3D11Resource* resource_interface = nullptr;
    ID3D11Texture2D* texture = nullptr;
    depth_target->GetResource(&resource_interface);
    if (resource_interface == nullptr || FAILED(resource_interface->QueryInterface(
            IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture))) ||
        texture == nullptr) {
        if (resource_interface != nullptr) {
            resource_interface->Release();
        }
        return;
    }

    D3D11_TEXTURE2D_DESC description = {};
    texture->GetDesc(&description);
    const UINT reference_width =
        g_backbuffer_width.load(std::memory_order_acquire);
    const UINT reference_height =
        g_backbuffer_height.load(std::memory_order_acquire);

    AcquireSRWLockExclusive(&g_observer_lock);
    if (g_discovery_active.load(std::memory_order_relaxed)) {
        UINT resource_index = find_resource(texture);
        bool new_resource = false;
        if (resource_index == kInvalidResourceIndex) {
            resource_index = select_resource_slot(
                description, reference_width, reference_height);
            new_resource = resource_index != kInvalidResourceIndex;
        }
        if (resource_index != kInvalidResourceIndex) {
            ObservedDepthResource& resource =
                g_observed_resources[resource_index];
            if (new_resource) {
                if (resource.generation == 0) {
                    resource.generation = 1;
                }
                resource.identity = texture;
                texture->AddRef();
                resource.texture = texture;
                resource.description = description;
                resource.width = description.Width;
                resource.height = description.Height;
                resource.texture_format = description.Format;
                resource.view_format = view_description.Format;
                resource.sample_count = description.SampleDesc.Count;
                resource.bind_flags = description.BindFlags;
                resource.bindings = 1;
                resource.observed_views = 1;
            } else {
                InterlockedIncrement64(&resource.bindings);
                ++resource.observed_views;
            }
            cache_view(
                depth_target, resource_index, resource.generation);
        }
    }
    ReleaseSRWLockExclusive(&g_observer_lock);

    texture->Release();
    resource_interface->Release();
    finish_discovery_if_due();
}

}
