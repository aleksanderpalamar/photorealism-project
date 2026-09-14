#pragma once

#include "bindings/setting_binding.hpp"
#include "theme.hpp"

#include <cstddef>

namespace photorealism {
namespace overlay {

inline float row_height(const MenuRow& row) {
    return row.kind == RowKind::Separator ? theme::kSeparatorHeight
                                          : theme::kRowHeight;
}

inline float row_top(const SettingPage& page, std::size_t index) {
    float top = 0.0f;
    for (std::size_t row = 0; row < index && row < page.count; ++row) {
        top += row_height(page.rows[row]) + theme::kRowGap;
    }
    return top;
}

inline float banner_height(const SettingPage& page) {
    return page.upscale_status ? theme::kRowHeight + theme::kRowGap : 0.0f;
}

inline float content_height_of(const SettingPage& page) {
    return banner_height(page) + row_top(page, page.count);
}

}
}
