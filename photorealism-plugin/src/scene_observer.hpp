#pragma once

#include "scene_features.hpp"

#include <d3d11.h>

namespace photorealism {

// Observador do frame pre-grade.
//
// A fonte OBRIGATORIA e a copia da cena, nunca a saida: o grade e funcao das
// features e as features seriam funcao do grade, o que fecha uma realimentacao
// e faz a cor caminhar sozinha. `observe` recebe a mesma textura que o passe
// visual le como entrada.
class SceneObserver {
  public:
    void configure(bool enabled, unsigned interval_frames, float log_seconds);
    void release();

    // Uma chamada por frame. So faz trabalho de GPU a cada `interval_frames`,
    // e a leitura volta alguns frames depois, sem travar o render.
    void observe(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11Texture2D* scene);

    SceneFeatures latest() const { return latest_; }

  private:
    // Dois slots bastam: um em voo e um livre. A cadencia de amostragem e de
    // dezenas de frames, entao nunca ha mais de uma copia pendente.
    struct Slot {
        ID3D11Texture2D* staging;
        ID3D11Query* completion;
        bool pending;
    };

    bool ensure_resources(
        ID3D11Device* device, ID3D11Texture2D* scene);
    void drain(ID3D11DeviceContext* context);
    void compute(const unsigned char* pixels, unsigned pitch);

    bool enabled_ = false;
    unsigned interval_frames_ = 30u;
    float log_seconds_ = 30.0f;

    ID3D11Texture2D* pyramid_ = nullptr;
    ID3D11ShaderResourceView* pyramid_view_ = nullptr;
    unsigned source_width_ = 0u;
    unsigned source_height_ = 0u;
    DXGI_FORMAT source_format_ = DXGI_FORMAT_UNKNOWN;
    unsigned mip_level_ = 0u;
    unsigned mip_width_ = 0u;
    unsigned mip_height_ = 0u;
    bool resources_failed_ = false;

    Slot slots_[2] = {};
    unsigned next_slot_ = 0u;
    unsigned frame_counter_ = 0u;

    SceneFeatures latest_ = {};
    unsigned long long last_log_ms_ = 0ull;
    bool first_measurement_logged_ = false;
};

}  // namespace photorealism
