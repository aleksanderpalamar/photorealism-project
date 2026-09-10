#pragma once

#include "features.hpp"
#include "sample_sink.hpp"
#include "sampler.hpp"

#include <d3d11.h>

namespace photorealism {

class SceneObserver : public SceneSampleSink {
  public:
    void configure(bool enabled, unsigned interval_frames, float log_seconds);
    void release();

    void observe(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11Texture2D* scene);

    SceneFeatures latest() const { return latest_; }

    void on_sample(
        const unsigned char* pixels,
        unsigned width,
        unsigned height,
        unsigned pitch,
        bool bgra) override;

  private:
    SceneSampler sampler_;
    bool enabled_ = false;
    unsigned interval_frames_ = 30u;
    float log_seconds_ = 30.0f;
    unsigned frame_counter_ = 0u;
    SceneFeatures latest_ = {};
    unsigned long long last_log_ms_ = 0ull;
    bool first_measurement_logged_ = false;
};

}  // namespace photorealism
