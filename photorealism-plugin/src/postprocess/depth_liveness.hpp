#pragma once

#include <cstdint>

namespace photorealism {

class DepthLiveness {
  public:
    void reset();
    bool is_safe_for_scene(
        std::uint64_t generation, std::uint64_t serial, bool* expired);

  private:
    void note_active(std::uint64_t generation, std::uint64_t serial);
    bool note_idle(std::uint64_t generation, std::uint64_t serial);

    static constexpr unsigned kActivityGraceFrames = 2;
    static constexpr unsigned kStaleFrameThreshold = 30;

    std::uint64_t generation_ = 0;
    std::uint64_t last_binding_serial_ = 0;
    unsigned stale_frame_count_ = 0;
    bool stale_logged_ = false;
};

}  // namespace photorealism
