#include "profile_switch.hpp"

#include "config.hpp"
#include "grade_fields.hpp"
#include "profile_layer.hpp"

namespace photorealism {

void carry_profile_choice(CalibrationStack* stack, const Settings* live) {
    if (stack == nullptr || live == nullptr) {
        return;
    }
    stack->modules.photorealism_profile_enabled =
        live->photorealism_profile_enabled;
    stack->modules.profile_tonemap_set = live->profile_tonemap_set;
}

bool profile_choice_changed(const Settings& live) {
    const float wanted =
        live.photorealism_profile_enabled
            ? static_cast<float>(tonemap_set_number(live.profile_tonemap_set))
            : 0.0f;
    return wanted != live.profile_active_set;
}

bool switch_grade_profile(Settings* live) {
    if (live == nullptr || !profile_choice_changed(*live)) {
        return false;
    }
    CalibrationStack stack = {};
    load_stack(&stack);
    carry_profile_choice(&stack, live);
    const Settings composed = compose(stack);
    copy_grade_fields(live, composed);
    copy_profile_outputs(live, composed);
    return true;
}

}
