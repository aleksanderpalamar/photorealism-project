#pragma once

#include "settings.hpp"

#include <cstddef>

namespace photorealism {

enum class PendingReason {
    GBuffer,
    GameHdr,
    MotionVectors,
    MirrorTarget,
    ShaderSwap,
    Fxaa,
    AlreadyMet,
    NoEquivalent,
};

constexpr std::size_t kPendingReasonCount = 8;

struct PendingProfileKey {
    const char* key;
    float Settings::*member;
    float reference;
    PendingReason reason;
};

extern const PendingProfileKey kPendingProfileKeys[];
extern const std::size_t kPendingProfileKeyCount;

const char* pending_reason_text(PendingReason reason);
bool apply_pending_key(Settings* modules, const char* key, const char* value);
void apply_pending_references(Settings* modules);

}
