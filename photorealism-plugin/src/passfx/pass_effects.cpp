#include "pass_effects.hpp"

#include "../postprocess/com_utils.hpp"
#include "../runtime.hpp"

#include <windows.h>

#include <cmath>

namespace photorealism {
namespace passfx {
namespace {

constexpr unsigned long long kReportWindowMs = 10000ull;

BindShape describe(UINT count, ID3D11RenderTargetView* const* targets) {
    BindShape shape;
    shape.count = targets != nullptr ? count : 0u;
    ID3D11RenderTargetView* first = shape.count > 0 ? targets[0] : nullptr;
    if (first == nullptr) {
        return shape;
    }
    D3D11_RENDER_TARGET_VIEW_DESC view = {};
    first->GetDesc(&view);
    shape.view_format = static_cast<unsigned>(view.Format);
    ID3D11Resource* resource = nullptr;
    first->GetResource(&resource);
    ID3D11Texture2D* texture = nullptr;
    if (resource != nullptr) {
        resource->QueryInterface(IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture));
        resource->Release();
    }
    D3D11_TEXTURE2D_DESC description = {};
    if (texture != nullptr) {
        texture->GetDesc(&description);
        texture->Release();
    }
    shape.width = description.Width;
    shape.height = description.Height;
    return shape;
}

bool same_parameters(const PreToneParameters& left, const PreToneParameters& right) {
    return left.exposure_gain == right.exposure_gain && left.contrast == right.contrast;
}

}

bool PassEffects::create(ID3D11Device* device) {
    release();
    return pre_tone_.create(device);
}

void PassEffects::release() {
    pre_tone_.release();
    safe_release(previous_hdr_);
    tone_.reset();
    announced_ = false;
}

void PassEffects::remember_hdr(
    ID3D11RenderTargetView* view, const BindShape& shape, bool is_hdr) {
    safe_release(previous_hdr_);
    previous_shape_ = shape;
    if (is_hdr && view != nullptr) {
        view->AddRef();
        previous_hdr_ = view;
    }
}

void PassEffects::note_application(const PreToneParameters& parameters) {
    ++applied_;
    if (announced_ && same_parameters(parameters, logged_)) {
        return;
    }
    log_message(
        "Pre-tom 0.24.1 ativo: ganho=%.3f (%+.2f EV) contraste=%.2f no HDR %ux%u "
        "antes do tom do jogo.",
        static_cast<double>(parameters.exposure_gain),
        static_cast<double>(std::log2(parameters.exposure_gain)),
        static_cast<double>(parameters.contrast), previous_shape_.width,
        previous_shape_.height);
    logged_ = parameters;
    announced_ = true;
}

void PassEffects::report_window() {
    const unsigned long long now = GetTickCount64();
    if (window_start_ == 0ull) {
        window_start_ = now;
        return;
    }
    if (now - window_start_ < kReportWindowMs) {
        return;
    }
    log_message(
        applied_ > 0
            ? "Pre-tom 0.24.1: %u quadros aplicados nos ultimos 10 s."
            : "Pre-tom 0.24.1 ligado, mas o passe do tom nao apareceu nos ultimos "
              "10 s (%u quadros).",
        applied_);
    applied_ = 0;
    window_start_ = now;
}

void PassEffects::observe(
    ID3D11DeviceContext* context,
    UINT render_target_count,
    ID3D11RenderTargetView* const* render_targets,
    const Settings& settings,
    ID3D11VertexShader* vertex_shader) {
    const PreToneParameters parameters = pre_tone_parameters(settings);
    const BindShape shape = describe(render_target_count, render_targets);
    const bool tone_output = tone_.observe(shape);
    if (tone_output && pre_tone_.apply(context, previous_hdr_, vertex_shader, parameters)) {
        note_application(parameters);
    }
    report_window();
    remember_hdr(shape.count > 0 ? render_targets[0] : nullptr, shape, tone_.previous_is_hdr());
}

}
}
