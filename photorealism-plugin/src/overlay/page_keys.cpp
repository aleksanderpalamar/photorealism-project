#include "overlay.hpp"
#include "page_metrics.hpp"

namespace photorealism {
namespace overlay {
namespace {

std::size_t next_selectable(
    const SettingPage& page, std::size_t current, int delta) {
    const long long total = static_cast<long long>(page.count);
    long long moved = static_cast<long long>(current);
    for (long long step = 0; step < total; ++step) {
        moved = ((moved + delta) % total + total) % total;
        if (row_is_selectable(page.rows[moved])) {
            return static_cast<std::size_t>(moved);
        }
    }
    return current;
}

int vertical_direction(unsigned keys) {
    return ((keys & kKeyDown) != 0 ? 1 : 0) - ((keys & kKeyUp) != 0 ? 1 : 0);
}

int horizontal_direction(unsigned keys) {
    return ((keys & kKeyRight) != 0 ? 1 : 0) - ((keys & kKeyLeft) != 0 ? 1 : 0);
}

}

void Menu::step_selection(const SettingPage& page, const Rect& body) {
    if ((keys_ & kKeyTab) != 0) {
        open_page(page.parent);
        return;
    }
    const int vertical = vertical_direction(keys_);
    const bool invalid =
        selected_ >= page.count || !row_is_selectable(page.rows[selected_]);
    if (vertical != 0 || invalid) {
        const std::size_t start = invalid ? page.count - 1 : selected_;
        selected_ = next_selectable(page, start, vertical != 0 ? vertical : 1);
        column_ = 0;
    }

    const float top = row_top(page, selected_) + banner_height(page);
    const float bottom = top + theme::kRowHeight;
    scroll_ = top < scroll_ ? top : scroll_;
    scroll_ = bottom > scroll_ + body.height ? bottom - body.height : scroll_;

    const MenuRow& row = page.rows[selected_];
    const bool setting =
        row.kind == RowKind::Setting || row.kind == RowKind::Pair;
    if (!setting) {
        if ((keys_ & kKeyEnter) != 0) {
            activate_row(row);
        }
        return;
    }
    if (step_pair_column(row)) {
        return;
    }
    step_binding(column_ == 1 ? row.second : row.first);
}

bool Menu::step_pair_column(const MenuRow& row) {
    if (row.kind != RowKind::Pair) {
        column_ = 0;
        return false;
    }
    const int direction = horizontal_direction(keys_);
    if (direction == 0) {
        return false;
    }
    column_ = direction > 0 ? 1 : 0;
    return true;
}

void Menu::step_binding(const SettingBinding& binding) {
    const bool enter = (keys_ & kKeyEnter) != 0;
    const float current = binding_value(binding, *settings_);
    if (binding.kind == BindingKind::Toggle) {
        if (enter) {
            set_binding_flag(
                binding, settings_, !binding_flag(binding, *settings_));
            apply_change(binding);
        }
        return;
    }
    if (enter && binding.kind == BindingKind::Choice) {
        set_value(binding, cycled_binding_value(binding, current));
        return;
    }
    if (enter) {
        reset_binding(binding);
        return;
    }
    const int direction = horizontal_direction(keys_);
    if (direction != 0) {
        set_value(binding, stepped_binding_value(binding, current, direction));
    }
}

void Menu::activate_row(const MenuRow& row) {
    if (row.kind == RowKind::Link) {
        open_page(row.target);
        return;
    }
    if (row.kind == RowKind::Hide) {
        hide();
        return;
    }
    if (row.kind == RowKind::Restore) {
        restore_defaults();
    }
}

}
}
