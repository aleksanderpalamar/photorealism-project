#include "setting_binding.hpp"

namespace photorealism {
namespace overlay {

const SettingBinding kProfileBindings[] = {
    {"Perfil photorealism", BindingKind::Toggle, nullptr, &Settings::photorealism_profile_enabled, 0.0f, 0.0f, 0, false, false},
};

const std::size_t kProfileBindingCount =
    sizeof(kProfileBindings) / sizeof(kProfileBindings[0]);

bool binding_switches_profile(const SettingBinding& binding) {
    return binding.flag == &Settings::photorealism_profile_enabled;
}

}
}
