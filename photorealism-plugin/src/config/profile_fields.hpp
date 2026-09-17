#pragma once

#include "module_field.hpp"
#include "settings.hpp"

#include <cstddef>

namespace photorealism {

constexpr const char kProfileSection[] = "profile.photorealism.0.23.0";

enum class PendingReason {
    GBuffer,
    MotionVectors,
    MirrorTarget,
    ShaderSwap,
    NoEquivalent,
};

constexpr std::size_t kPendingReasonCount = 5;

struct PendingControl {
    float Settings::*member;
    PendingReason reason;
};

constexpr std::size_t kProfileFieldCount = 29;

extern const ModuleField kProfileFields[kProfileFieldCount];
extern const PendingControl kPendingControls[];
extern const std::size_t kPendingControlCount;

const char* pending_reason_text(PendingReason reason);
const char* profile_key_for(float Settings::*member);

}
