#include "profile_switch.hpp"

#include "config.hpp"
#include "grade_fields.hpp"

namespace photorealism {

void switch_grade_profile(Settings* live) {
    if (live == nullptr) {
        return;
    }
    CalibrationStack stack = {};
    load_stack(&stack);
    stack.modules.photorealism_profile_enabled =
        live->photorealism_profile_enabled;
    const Settings composed = compose(stack);
    copy_grade_fields(live, composed);
    live->ssao_intensity_scale = composed.ssao_intensity_scale;
    live->condition_color_locked = composed.condition_color_locked;
}

}
