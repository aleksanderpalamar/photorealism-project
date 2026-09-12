#include "persistence.hpp"

#include "../config/config.hpp"
#include "../config/file_io.hpp"
#include "../config/section_table.hpp"
#include "../config/writer.hpp"
#include "../runtime.hpp"
#include "bindings/setting_binding.hpp"
#include "grade_keys.hpp"

#include <cstdio>
#include <string>

namespace photorealism {
namespace overlay {
namespace {

constexpr const wchar_t* kTemporaryName = L"photorealism-plugin.menu.tmp";

void format_number(float value, int decimals, char* buffer, std::size_t size) {
    std::snprintf(buffer, size, "%.*f", decimals, static_cast<double>(value));
}

bool write_grade_delta(
    std::string* text,
    const SettingBinding& binding,
    const Settings& settings,
    const Settings& baseline) {
    const char* key = grade_key_for(binding.number);
    if (key == nullptr) {
        return false;
    }
    const float delta =
        settings.*(binding.number) - baseline.*(binding.number);
    char printed[32] = {};
    format_number(delta, 6, printed, sizeof(printed));
    return config_writer::set_value(text, kUserSection, key, printed);
}

bool write_module_number(
    std::string* text, const SettingBinding& binding, const Settings& settings) {
    const char* section = nullptr;
    const char* key = nullptr;
    if (!locate_number(binding.number, &section, &key)) {
        return false;
    }
    char printed[32] = {};
    format_number(
        settings.*(binding.number), binding.decimals, printed, sizeof(printed));
    return config_writer::set_value(text, section, key, printed);
}

bool write_module_flag(
    std::string* text, const SettingBinding& binding, const Settings& settings) {
    const char* section = nullptr;
    if (!locate_flag(binding.flag, &section)) {
        return false;
    }
    return config_writer::set_value(
        text, section, "enabled", settings.*(binding.flag) ? "true" : "false");
}

bool differs(
    const SettingBinding& binding,
    const Settings& settings,
    const Settings& on_disk) {
    if (binding.kind == BindingKind::Toggle) {
        return settings.*(binding.flag) != on_disk.*(binding.flag);
    }
    return settings.*(binding.number) != on_disk.*(binding.number);
}

int write_page(
    std::string* text,
    const SettingPage& page,
    const Settings& settings,
    const Settings& baseline,
    const Settings& on_disk) {
    int changed = 0;
    for (std::size_t index = 0; index < page.count; ++index) {
        const SettingBinding& binding = page.items[index];
        if (binding.inert) {
            continue;
        }
        if (!differs(binding, settings, on_disk)) {
            continue;
        }
        const bool wrote =
            binding.kind == BindingKind::Toggle
                ? write_module_flag(text, binding, settings)
                : (binding.grade
                       ? write_grade_delta(text, binding, settings, baseline)
                       : write_module_number(text, binding, settings));
        changed += wrote ? 1 : 0;
    }
    return changed;
}

}

Settings measured_baseline(const CalibrationStack& stack) {
    CalibrationStack without_user = stack;
    without_user.user_0_20 = CalibrationLayer{};
    return compose(without_user);
}

SaveReport save_settings(
    const Settings& settings,
    const Settings& baseline,
    const Settings& on_disk) {
    SaveReport report;
    std::string text;
    if (!config_io::read_file(config_path(), &text)) {
        log_message("Menu nao conseguiu ler o cfg para gravar.");
        return report;
    }

    const SettingPage* pages = setting_pages();
    for (std::size_t index = 0; index < setting_page_count(); ++index) {
        report.changed +=
            write_page(&text, pages[index], settings, baseline, on_disk);
    }
    if (report.changed == 0) {
        report.written = true;
        log_message("Menu: nada a gravar, nenhum ajuste mudou.");
        return report;
    }

    report.written =
        config_io::write_atomic(config_path(), kTemporaryName, text);
    log_message(
        "Menu gravou %d ajuste(s) no cfg: %s.",
        report.changed,
        report.written ? "ok" : "falhou");
    return report;
}

}
}
