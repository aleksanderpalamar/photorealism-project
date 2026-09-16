#pragma once

#include <string>

#include "../native_aa/config_text.hpp"
#include "quality_settings.hpp"

namespace photorealism {
namespace native_quality {

inline QualitySnapshot read_snapshot(const std::string& contents) {
    QualitySnapshot snapshot = {};
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        snapshot.detected[index] =
            aa_config::config_value(contents, kSettings[index].key);
    }
    return snapshot;
}

inline bool needs_change(
    const QualitySnapshot& snapshot,
    const QualityPolicy& policy,
    std::size_t index) {
    return snapshot.detected[index] != kAbsent &&
           snapshot.detected[index] != policy.desired[index];
}

inline unsigned count_changes(
    const QualitySnapshot& snapshot, const QualityPolicy& policy) {
    unsigned total = 0;
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        if (needs_change(snapshot, policy, index)) {
            ++total;
        }
    }
    return total;
}

inline unsigned count_absent(const QualitySnapshot& snapshot) {
    unsigned total = 0;
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        if (snapshot.detected[index] == kAbsent) {
            ++total;
        }
    }
    return total;
}

inline std::string describe(
    const QualitySnapshot& snapshot,
    const QualityPolicy& policy,
    bool only_changes) {
    std::string text;
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        if (only_changes && !needs_change(snapshot, policy, index)) {
            continue;
        }
        if (!text.empty()) {
            text += ' ';
        }
        text += kSettings[index].key;
        text += '=';
        text += snapshot.detected[index];
        if (only_changes) {
            text += "->";
            text += policy.desired[index];
        }
    }
    return text.empty() ? std::string("nenhuma") : text;
}

inline unsigned apply_settings(
    std::string* contents,
    const QualitySnapshot& snapshot,
    const QualityPolicy& policy) {
    unsigned applied = 0;
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        if (!needs_change(snapshot, policy, index)) {
            continue;
        }
        if (aa_config::set_config_value(
                contents,
                kSettings[index].key,
                policy.desired[index].c_str())) {
            ++applied;
        }
    }
    return applied;
}

inline QualityPolicy policy_for(QualityLevel level) {
    QualityPolicy policy = {};
    policy.manage = true;
    policy.level = level;
    for (std::size_t index = 0; index < kSettingCount; ++index) {
        policy.desired[index] = value_for(kSettings[index], level);
    }
    return policy;
}

}
}
