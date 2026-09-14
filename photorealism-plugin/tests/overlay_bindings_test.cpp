#include <windows.h>

namespace photorealism {
const wchar_t* config_path() { return L"/tmp/photorealism-overlay-bindings.cfg"; }
void log_message(const char*, ...) {}
}

#include "../src/overlay/bindings/binding_values.cpp"
#include "../src/overlay/bindings/menu_pages.cpp"

#include "config/config.hpp"
#include "config/defaults.hpp"
#include "config/profile_fields.hpp"
#include "config/profile_state.hpp"
#include "config/section_table.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace photorealism;
using namespace photorealism::overlay;

namespace {

template <typename Visit>
void for_each_binding(Visit visit) {
    for (std::size_t page = 0; page < setting_page_count(); ++page) {
        const SettingPage& current = setting_pages()[page];
        for (std::size_t index = 0; index < current.count; ++index) {
            const MenuRow& row = current.rows[index];
            if (row.kind == RowKind::Setting || row.kind == RowKind::Pair) {
                visit(row.first);
            }
            if (row.kind == RowKind::Pair) {
                visit(row.second);
            }
        }
    }
}

bool stored_in_the_tonemap_set(const SettingBinding& binding) {
    Settings settings = default_settings();
    const PhotorealismTonemap before = active_tonemap(settings);
    settings.*(binding.number) += 0.5f;
    store_active_tonemap(&settings);
    return std::memcmp(&before, &active_tonemap(settings), sizeof(before)) != 0;
}

void every_setting_is_wired_and_can_be_saved() {
    for_each_binding([](const SettingBinding& binding) {
        assert(binding.label != nullptr);
        assert((binding.flag != nullptr) != (binding.number != nullptr));
        const char* section = nullptr;
        const char* key = nullptr;
        if (binding.flag != nullptr) {
            assert(binding.kind == BindingKind::Toggle);
            assert(locate_flag(binding.flag, &section));
            return;
        }
        assert(binding.minimum < binding.maximum);
        assert((binding.kind == BindingKind::Choice) == (binding.choices != nullptr));
        if (binding.tonemap) {
            assert(stored_in_the_tonemap_set(binding));
            return;
        }
        assert(locate_number(binding.number, &section, &key));
    });
}

std::size_t rows_for(float Settings::*member) {
    std::size_t rows = 0;
    for_each_binding([&](const SettingBinding& binding) {
        rows += binding.number == member ? 1 : 0;
    });
    return rows;
}

constexpr float Settings::*kCfgOnlyKeys[] = {
    &Settings::profile_taa_level,
    &Settings::profile_dlss_preset,
    &Settings::profile_color_preset,
    &Settings::profile_color_preset_extra_brightness,
    &Settings::profile_tonemap_operator,
    &Settings::profile_tonemap_operator_a,
    &Settings::profile_hide_show_key,
};

bool cfg_only(float Settings::*member) {
    for (float Settings::*key : kCfgOnlyKeys) {
        if (key == member) {
            return true;
        }
    }
    return false;
}

void every_profile_key_on_the_reference_screens_has_one_control() {
    for (std::size_t index = 0; index < kProfileFieldCount; ++index) {
        float Settings::*member = kProfileFields[index].member;
        assert(rows_for(member) == (cfg_only(member) ? 0u : 1u));
    }
}

void every_page_is_reachable_and_goes_back_to_its_parent() {
    bool reached[16] = {};
    reached[kPageMain] = true;
    for (std::size_t page = 0; page < setting_page_count(); ++page) {
        const SettingPage& current = setting_pages()[page];
        bool goes_back = page == kPageMain;
        for (std::size_t index = 0; index < current.count; ++index) {
            const MenuRow& row = current.rows[index];
            if (row.kind != RowKind::Link) {
                continue;
            }
            assert(row.target < setting_page_count());
            reached[row.target] = true;
            goes_back = goes_back || row.target == current.parent;
        }
        assert(goes_back);
    }
    for (std::size_t page = 0; page < setting_page_count(); ++page) {
        assert(reached[page]);
        assert(std::strcmp(setting_pages()[page].title, "Render") != 0);
        assert(std::strcmp(setting_pages()[page].title, "Clima") != 0);
    }
}

void the_main_page_starts_with_lighting_and_quality() {
    const MenuRow& first = setting_pages()[kPageMain].rows[0];
    assert(first.kind == RowKind::Pair);
    assert(first.first.number == &Settings::profile_lighting_method);
    assert(first.first.kind == BindingKind::Choice);
    assert(choice_count(first.first) == 4);
    assert(std::strcmp(first.first.choices[3], "Iluminacao D (padrao)") == 0);
    assert(first.second.number == &Settings::profile_global_quality);
    assert(choice_count(first.second) == 3);
}

void no_slider_chooses_between_options() {
    for_each_binding([](const SettingBinding& binding) {
        const bool is_switch = binding.kind == BindingKind::Toggle;
        const bool is_choice = binding.kind == BindingKind::Choice;
        const bool integer_list = binding.decimals == 0 && binding.maximum <= 6.0f;
        assert(is_switch || is_choice || !integer_list);
    });
}

void a_dropdown_moves_one_option_at_a_time() {
    const SettingBinding& lighting = setting_pages()[kPageMain].rows[0].first;
    assert(choice_index(lighting, 3.0f) == 3);
    assert(choice_index(lighting, 9.0f) == 3);
    assert(choice_index(lighting, -2.0f) == 0);
    assert(stepped_binding_value(lighting, 2.0f, 1) == 3.0f);
    assert(stepped_binding_value(lighting, 3.0f, 1) == 3.0f);
    assert(stepped_binding_value(lighting, 0.0f, -1) == 0.0f);
    assert(cycled_binding_value(lighting, 3.0f) == 0.0f);
    assert(cycled_binding_value(lighting, 1.0f) == 2.0f);
}

void a_zero_or_one_key_is_a_switch() {
    const SettingBinding& fxaa = setting_pages()[kPageAntiAliasing].rows[1].first;
    assert(fxaa.number == &Settings::profile_fxaa);
    assert(fxaa.kind == BindingKind::Toggle);
    Settings settings = {};
    settings.profile_fxaa = 1.0f;
    assert(binding_flag(fxaa, settings));
    set_binding_flag(fxaa, &settings, false);
    assert(settings.profile_fxaa == 0.0f && !binding_flag(fxaa, settings));
}

void integer_sliders_snap_to_whole_numbers() {
    const SettingBinding& sharpness = setting_pages()[kPageAntiAliasing].rows[2].first;
    assert(sharpness.number == &Settings::profile_sharpness);
    assert(quantized_binding_value(sharpness, 6.37f) == 6.0f);
    assert(quantized_binding_value(sharpness, 6.51f) == 7.0f);
    assert(quantized_binding_value(sharpness, 14.0f) == 10.0f);
}

}

int main() {
    every_setting_is_wired_and_can_be_saved();
    every_profile_key_on_the_reference_screens_has_one_control();
    every_page_is_reachable_and_goes_back_to_its_parent();
    the_main_page_starts_with_lighting_and_quality();
    no_slider_chooses_between_options();
    a_dropdown_moves_one_option_at_a_time();
    a_zero_or_one_key_is_a_switch();
    integer_sliders_snap_to_whole_numbers();
    std::printf("overlay_bindings_test ok\n");
    return 0;
}
