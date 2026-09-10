#include "depth_liveness.hpp"

#include "../resource_observer.hpp"
#include "../runtime.hpp"

namespace photorealism {

void DepthLiveness::reset() {
    generation_ = 0;
    last_binding_serial_ = 0;
    stale_frame_count_ = 0;
    stale_logged_ = false;
}

void DepthLiveness::note_active(
    std::uint64_t generation, std::uint64_t serial) {
    if (stale_logged_) {
        log_message(
            "Depth voltou a ser usado pela cena: generation=%llu; "
            "SSAO photorealista retomado.",
            static_cast<unsigned long long>(generation));
    }
    last_binding_serial_ = serial;
    stale_frame_count_ = 0;
    stale_logged_ = false;
}

bool DepthLiveness::note_idle(
    std::uint64_t generation, std::uint64_t serial) {
    if (stale_frame_count_ < kStaleFrameThreshold) {
        ++stale_frame_count_;
    }
    const bool grace_just_expired =
        stale_frame_count_ == kActivityGraceFrames + 1 &&
        !stale_logged_;
    if (grace_just_expired) {
        log_message(
            "Depth sem atividade confirmada por %u frames: "
            "generation=%llu; SSAO suspenso e visual "
            "photorealista preservado.",
            stale_frame_count_,
            static_cast<unsigned long long>(generation));
        stale_logged_ = true;
    }

    const bool expired =
        stale_frame_count_ >= kStaleFrameThreshold &&
        invalidate_stale_depth_candidate(generation, serial);
    if (!expired) {
        return false;
    }
    log_message(
        "Depth obsoleto invalidado apos %u frames; "
        "redescoberta automatica iniciada sem acao do usuario.",
        kStaleFrameThreshold);
    return true;
}

bool DepthLiveness::is_safe_for_scene(
    std::uint64_t generation, std::uint64_t serial, bool* expired) {
    if (generation_ != generation) {
        generation_ = generation;
        last_binding_serial_ = 0;
        stale_frame_count_ = 0;
        stale_logged_ = false;
    }

    const bool used_by_current_scene =
        serial != 0 && serial != last_binding_serial_;
    if (used_by_current_scene) {
        note_active(generation, serial);
        return true;
    }
    *expired = note_idle(generation, serial);
    if (*expired) {
        return false;
    }
    return stale_frame_count_ <= kActivityGraceFrames;
}

}  // namespace photorealism
