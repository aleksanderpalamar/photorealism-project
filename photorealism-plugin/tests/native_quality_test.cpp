#include "../src/native_quality/quality_plan.hpp"

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <set>
#include <string>

using namespace photorealism::native_quality;

namespace {

std::string sample_config() {
    std::string text = "uset g_console \"1\"\n";
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        text += "uset ";
        text += kSettings[index].key;
        text += " \"";
        text += kSettings[index].low;
        text += "\"\n";
    }
    text += "uset r_aa \"6\"\n";
    return text;
}

bool contains(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

void test_every_key_is_unique_and_named() {
    std::set<std::string> seen;
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        const QualitySetting& setting = kSettings[index];
        assert(setting.key != nullptr && setting.key[0] != '\0');
        assert(setting.high != nullptr && setting.medium != nullptr);
        assert(setting.low != nullptr);
        assert(seen.insert(setting.key).second);
        const std::string key = setting.key;
        assert(key.compare(0, 2, "r_") == 0 || key.compare(0, 2, "g_") == 0);
    }
}

void test_levels_are_ordered() {
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        const QualitySetting& setting = kSettings[index];
        const double high = std::atof(setting.high);
        const double medium = std::atof(setting.medium);
        const double low = std::atof(setting.low);
        if (setting.lower_is_better) {
            assert(high <= medium && medium <= low);
        } else {
            assert(high >= medium && medium >= low);
        }
        assert(high != low || std::string(setting.high) == setting.low);
    }
}

void test_high_preset_raises_a_low_config() {
    std::string contents = sample_config();
    const QualityPolicy policy = policy_for(QualityLevel::high);
    const QualitySnapshot snapshot = read_snapshot(contents);

    assert(count_absent(snapshot) == 0);
    const unsigned pending = count_changes(snapshot, policy);
    assert(pending > 0);

    const unsigned applied = apply_settings(&contents, snapshot, policy);
    assert(applied == pending);

    const QualitySnapshot after = read_snapshot(contents);
    assert(count_changes(after, policy) == 0);
    assert(contains(contents, "uset r_texture_detail \"0\""));
    assert(contains(contents, "uset r_anisotropy_factor \"4\""));
    assert(contains(contents, "uset r_normal_maps \"1\""));
}

void test_low_preset_is_a_fixed_point() {
    std::string contents = sample_config();
    const QualityPolicy policy = policy_for(QualityLevel::low);
    const QualitySnapshot snapshot = read_snapshot(contents);
    assert(count_changes(snapshot, policy) == 0);
    assert(apply_settings(&contents, snapshot, policy) == 0);
    assert(contents == sample_config());
}

void test_untouched_keys_survive() {
    std::string contents = sample_config();
    const QualityPolicy policy = policy_for(QualityLevel::high);
    const QualitySnapshot snapshot = read_snapshot(contents);
    apply_settings(&contents, snapshot, policy);
    assert(contains(contents, "uset g_console \"1\""));
    assert(contains(contents, "uset r_aa \"6\""));
}

void test_absent_keys_are_never_created() {
    std::string contents = "uset g_console \"1\"\n";
    const QualityPolicy policy = policy_for(QualityLevel::high);
    const QualitySnapshot snapshot = read_snapshot(contents);
    assert(count_absent(snapshot) == kSettingCount);
    assert(count_changes(snapshot, policy) == 0);
    assert(apply_settings(&contents, snapshot, policy) == 0);
    assert(contents == "uset g_console \"1\"\n");
}

void test_each_level_reaches_its_own_target() {
    for (QualityLevel level :
         {QualityLevel::high, QualityLevel::medium, QualityLevel::low}) {
        std::string contents = sample_config();
        const QualityPolicy policy = policy_for(level);
        const QualitySnapshot snapshot = read_snapshot(contents);
        apply_settings(&contents, snapshot, policy);
        assert(count_changes(read_snapshot(contents), policy) == 0);
    }
}

void test_description_reports_the_transition() {
    std::string contents = sample_config();
    const QualityPolicy policy = policy_for(QualityLevel::high);
    const QualitySnapshot snapshot = read_snapshot(contents);
    const std::string text = describe(snapshot, policy, true);
    assert(contains(text.c_str(), "r_texture_detail=2->0"));
    assert(!contains(text, "nenhuma"));
}

}  // namespace

int main() {
    test_every_key_is_unique_and_named();
    test_levels_are_ordered();
    test_high_preset_raises_a_low_config();
    test_low_preset_is_a_fixed_point();
    test_untouched_keys_survive();
    test_absent_keys_are_never_created();
    test_each_level_reaches_its_own_target();
    test_description_reports_the_transition();
    std::printf("native_quality_test: ok\n");
    return 0;
}
