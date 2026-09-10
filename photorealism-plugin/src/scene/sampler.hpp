#pragma once

#include "sample_sink.hpp"

#include <d3d11.h>

namespace photorealism {

class SceneSampler {
  public:
    void release();
    bool ensure_resources(ID3D11Device* device, ID3D11Texture2D* scene);
    void drain(ID3D11DeviceContext* context, SceneSampleSink* sink);
    bool submit(ID3D11DeviceContext* context, ID3D11Texture2D* scene);

    struct Info {
        unsigned source_width;
        unsigned source_height;
        unsigned source_format;
        unsigned sample_format;
        unsigned mip_level;
        unsigned mip_width;
        unsigned mip_height;
    };

    Info info() const;
    bool consume_creation_notice();

  private:
    struct Slot {
        ID3D11Texture2D* staging;
        ID3D11Query* completion;
        bool pending;
    };

    Slot slots_[2] = {};
    ID3D11Texture2D* pyramid_ = nullptr;
    ID3D11ShaderResourceView* pyramid_view_ = nullptr;
    unsigned source_width_ = 0u;
    unsigned source_height_ = 0u;
    DXGI_FORMAT source_format_ = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT sample_format_ = DXGI_FORMAT_UNKNOWN;
    unsigned mip_level_ = 0u;
    unsigned mip_width_ = 0u;
    unsigned mip_height_ = 0u;
    unsigned next_slot_ = 0u;
    bool resources_failed_ = false;
    bool creation_notice_ = false;
};

}  // namespace photorealism
