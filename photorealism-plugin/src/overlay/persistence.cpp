#include "persistence.hpp"

#include "../config/config.hpp"
#include "../config/file_io.hpp"
#include "../config/profile_fields.hpp"
#include "../config/section_table.hpp"
#include "../config/writer.hpp"
#include "../runtime.hpp"
#include "bindings/setting_binding.hpp"

#include <cstdio>
#include <string>

namespace photorealism {
namespace overlay {
namespace {

constexpr const wchar_t* kTemporaryName = L"photorealism-plugin.menu.tmp";

void format_number(float value, int decimals, char* buffer, std::size_t size) {
    std::snprintf(buffer, size, "%.*f", decimals, static_cast<double>(value));
}

bool write_number(
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

bool write_flag(
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
    if (binding.flag != nullptr) {
        return settings.*(binding.flag) != on_disk.*(binding.flag);
    }
    return settings.*(binding.number) != on_disk.*(binding.number);
}

int write_binding(
    std::string* text,
    const SettingBinding& binding,
    const Settings& settings,
    const Settings& on_disk) {
    const bool stored = binding.flag != nullptr || binding.number != nullptr;
    if (!stored || binding.tonemap || !differs(binding, settings, on_disk)) {
        return 0;
    }
    const bool wrote = binding.flag != nullptr
                           ? write_flag(text, binding, settings)
                           : write_number(text, binding, settings);
    return wrote ? 1 : 0;
}

int write_pages(
    std::string* text, const Settings& settings, const Settings& on_disk) {
    int changed = 0;
    const SettingPage* pages = setting_pages();
    for (std::size_t page = 0; page < setting_page_count(); ++page) {
        for (std::size_t index = 0; index < pages[page].count; ++index) {
            const MenuRow& row = pages[page].rows[index];
            changed += write_binding(text, row.first, settings, on_disk);
            changed += write_binding(text, row.second, settings, on_disk);
        }
    }
    return changed;
}

int write_tonemap_sets(
    std::string* text, const Settings& settings, const Settings& on_disk) {
    int changed = 0;
    for (unsigned set = 0; set < kProfileTonemapSets; ++set) {
        for (std::size_t index = 0; index < kTonemapFieldCount; ++index) {
            const TonemapField& field = kTonemapFields[index];
            const float value = settings.tonemap_sets[set].*(field.member);
            if (value == on_disk.tonemap_sets[set].*(field.member)) {
                continue;
            }
            char key[64] = {};
            std::snprintf(key, sizeof(key), "tonemap_%s_%u", field.name, set + 1);
            char printed[32] = {};
            format_number(value, field.decimals, printed, sizeof(printed));
            changed += config_writer::set_value(text, kProfileSection, key, printed)
                           ? 1
                           : 0;
        }
    }
    return changed;
}

}

SaveReport save_settings(const Settings& settings, const Settings& on_disk) {
    SaveReport report;
    std::string text;
    if (!config_io::read_file(config_path(), &text)) {
        log_message("Menu nao conseguiu ler o cfg para gravar.");
        return report;
    }

    report.changed = write_pages(&text, settings, on_disk) +
                     write_tonemap_sets(&text, settings, on_disk);
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
