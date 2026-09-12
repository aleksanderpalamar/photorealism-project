#include "setting_binding.hpp"

namespace photorealism {
namespace overlay {
namespace {

const SettingPage kPages[] = {
    {"Cor", "Cor e curva de tom", kGradeBindings, kGradeBindingCount},
    {"Render", "Renderizacao e iluminacao", kRenderBindings,
     kRenderBindingCount},
    {"Clima", "Clima e condicao", kConditionBindings, kConditionBindingCount},
    {"FSR", "Upscale FSR -- vale na proxima troca de resolucao",
     kUpscaleBindings, kUpscaleBindingCount},
    {"Cena", "Observador e profundidade", kObserverBindings,
     kObserverBindingCount},
};

constexpr std::size_t kPageCount = sizeof(kPages) / sizeof(kPages[0]);

}

const SettingPage* setting_pages() {
    return kPages;
}

std::size_t setting_page_count() {
    return kPageCount;
}

float binding_value(const SettingBinding& binding, const Settings& settings) {
    if (binding.number == nullptr) {
        return 0.0f;
    }
    return settings.*(binding.number);
}

void set_binding_value(
    const SettingBinding& binding, Settings* settings, float value) {
    if (binding.number == nullptr || settings == nullptr) {
        return;
    }
    settings->*(binding.number) = value;
}

bool binding_flag(const SettingBinding& binding, const Settings& settings) {
    if (binding.flag == nullptr) {
        return false;
    }
    return settings.*(binding.flag);
}

void set_binding_flag(
    const SettingBinding& binding, Settings* settings, bool value) {
    if (binding.flag == nullptr || settings == nullptr) {
        return;
    }
    settings->*(binding.flag) = value;
}

bool binding_touches_observer(const SettingBinding& binding) {
    for (std::size_t index = 0; index < kConditionBindingCount; ++index) {
        if (&kConditionBindings[index] == &binding) {
            return true;
        }
    }
    for (std::size_t index = 0; index < kObserverBindingCount; ++index) {
        if (&kObserverBindings[index] == &binding) {
            return true;
        }
    }
    return false;
}

}
}
