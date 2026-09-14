#include "../src/overlay/bindings/condition_bindings.cpp"
#include "../src/overlay/bindings/grade_bindings.cpp"
#include "../src/overlay/bindings/pages.cpp"
#include "../src/overlay/bindings/profile_bindings.cpp"
#include "../src/overlay/bindings/render_bindings.cpp"
#include "../src/overlay/bindings/upscale_bindings.cpp"
#include "../src/overlay/grade_keys.cpp"

#include "config/section_table.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace photorealism;
using namespace photorealism::overlay;

namespace {

void every_binding_is_wired_to_a_field() {
    for (std::size_t page = 0; page < setting_page_count(); ++page) {
        const SettingPage& current = setting_pages()[page];
        assert(current.count > 0);
        assert(current.tab != nullptr && current.title != nullptr);
        for (std::size_t index = 0; index < current.count; ++index) {
            const SettingBinding& binding = current.items[index];
            assert(binding.label != nullptr);
            if (binding.kind == BindingKind::Toggle) {
                assert(binding.flag != nullptr);
                assert(binding.number == nullptr);
                continue;
            }
            assert(binding.number != nullptr);
            assert(binding.flag == nullptr);
            assert(binding.minimum < binding.maximum);
            assert(binding.decimals >= 0 && binding.decimals <= 6);
        }
    }
}

void every_control_can_be_written_back() {
    for (std::size_t page = 0; page < setting_page_count(); ++page) {
        const SettingPage& current = setting_pages()[page];
        for (std::size_t index = 0; index < current.count; ++index) {
            const SettingBinding& binding = current.items[index];
            if (binding.inert) {
                continue;
            }
            if (binding.kind == BindingKind::Toggle) {
                const char* section = nullptr;
                assert(locate_flag(binding.flag, &section));
                assert(section != nullptr);
                continue;
            }
            if (binding.grade) {
                assert(grade_key_for(binding.number) != nullptr);
                continue;
            }
            const char* section = nullptr;
            const char* key = nullptr;
            assert(locate_number(binding.number, &section, &key));
            assert(section != nullptr && key != nullptr);
        }
    }
}

void the_user_layer_is_the_only_destination_for_colour() {
    assert(std::strcmp(kUserSection, "module.user.0.20.0") == 0);
    for (std::size_t index = 0; index < kGradeKeyCount; ++index) {
        const char* key = kGradeKeys[index].key;
        const std::size_t length = std::strlen(key);
        assert(length > 6);
        assert(std::strcmp(key + length - 6, "_delta") == 0);
    }
}

void grade_bindings_and_grade_keys_agree() {
    assert(kGradeBindingCount == kGradeKeyCount);
    for (std::size_t index = 0; index < kGradeBindingCount; ++index) {
        assert(kGradeBindings[index].grade);
        assert(grade_key_for(kGradeBindings[index].number) != nullptr);
    }
}

void module_bindings_are_never_marked_as_colour() {
    const SettingBinding* tables[] = {
        kRenderBindings, kConditionBindings, kObserverBindings,
        kUpscaleBindings};
    const std::size_t counts[] = {
        kRenderBindingCount, kConditionBindingCount, kObserverBindingCount,
        kUpscaleBindingCount};
    for (std::size_t table = 0; table < 4; ++table) {
        for (std::size_t index = 0; index < counts[table]; ++index) {
            assert(!tables[table][index].grade);
        }
    }
}

void the_observer_group_asks_for_a_reconfiguration() {
    assert(binding_touches_observer(kConditionBindings[0]));
    assert(binding_touches_observer(kObserverBindings[0]));
    assert(!binding_touches_observer(kGradeBindings[0]));
    assert(!binding_touches_observer(kRenderBindings[0]));
}

void the_inert_bloom_controls_are_marked() {
    std::size_t inert = 0;
    for (std::size_t index = 0; index < kRenderBindingCount; ++index) {
        inert += kRenderBindings[index].inert ? 1 : 0;
    }
    assert(inert == 2);
}

}

int main() {
    every_binding_is_wired_to_a_field();
    every_control_can_be_written_back();
    the_user_layer_is_the_only_destination_for_colour();
    grade_bindings_and_grade_keys_agree();
    module_bindings_are_never_marked_as_colour();
    the_observer_group_asks_for_a_reconfiguration();
    the_inert_bloom_controls_are_marked();
    std::printf("overlay_bindings_test ok\n");
    return 0;
}
