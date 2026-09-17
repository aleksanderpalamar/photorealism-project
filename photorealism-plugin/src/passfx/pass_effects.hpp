#pragma once

#include "../config/settings.hpp"
#include "pre_tone_effect.hpp"
#include "tone_stage.hpp"

#include <d3d11.h>

namespace photorealism {
namespace passfx {

class PassEffects {
  public:
    bool create(ID3D11Device* device);
    void release();

    void observe(
        ID3D11DeviceContext* context,
        UINT render_target_count,
        ID3D11RenderTargetView* const* render_targets,
        const Settings& settings,
        ID3D11VertexShader* vertex_shader);

  private:
    void remember_hdr(
        ID3D11RenderTargetView* view, const BindShape& shape, bool is_hdr);
    void note_application(const PreToneParameters& parameters);
    void report_window();

    ToneStage tone_;
    PreToneEffect pre_tone_;
    ID3D11RenderTargetView* previous_hdr_ = nullptr;
    BindShape previous_shape_;
    PreToneParameters logged_ = {1.0f, 0.0f};
    unsigned applied_ = 0;
    unsigned long long window_start_ = 0ull;
    bool announced_ = false;
};

}
}
