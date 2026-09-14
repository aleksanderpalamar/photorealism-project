#include "controls.hpp"

#include "../layout.hpp"
#include "../text.hpp"
#include "../theme.hpp"

#include <cstdio>

namespace photorealism {
namespace overlay {
namespace {

constexpr float kResetWidth = 24.0f;
constexpr float kValueWidth = 92.0f;
constexpr float kLabelFraction = 0.40f;
constexpr float kTrackHeight = 6.0f;
constexpr float kThumbWidth = 10.0f;

float clamp_to(const SettingBinding& binding, float value) {
    if (value < binding.minimum) {
        return binding.minimum;
    }
    if (value > binding.maximum) {
        return binding.maximum;
    }
    return value;
}

float value_from_pointer(
    const SettingBinding& binding, const Rect& track, float pointer_x) {
    if (track.width <= 0.0f) {
        return binding.minimum;
    }
    const float ratio = (pointer_x - track.x) / track.width;
    const float span = binding.maximum - binding.minimum;
    return clamp_to(binding, binding.minimum + ratio * span);
}

float ratio_of(const SettingBinding& binding, float value) {
    const float span = binding.maximum - binding.minimum;
    if (span <= 0.0f) {
        return 0.0f;
    }
    return (clamp_to(binding, value) - binding.minimum) / span;
}

void format_value(
    const SettingBinding& binding, float value, char* buffer, int size) {
    std::snprintf(buffer, static_cast<std::size_t>(size), "%.*f",
                  binding.decimals, static_cast<double>(value));
}

void draw_track(UiContext& ui, const Rect& track, float ratio) {
    ui.list->push_rect(track, theme::kTrack, theme::kTrackHeightRadius);
    const Rect filled = {track.x, track.y, track.width * ratio, track.height};
    ui.list->push_rect(filled, theme::kAccent, theme::kTrackHeightRadius);

    const Rect thumb = {
        track.x + track.width * ratio - kThumbWidth * 0.5f,
        track.y - 5.0f,
        kThumbWidth,
        track.height + 10.0f};
    ui.list->push_rect(thumb, theme::kThumb, theme::kControlRadius);
}

}

RowResult slider_row(
    UiContext& ui,
    const Rect& area,
    const SettingBinding& binding,
    float* value) {
    RowResult result;
    const Color label_color = theme::kText;
    const float text_top = area.y + (area.height - ui.font->line_height()) * 0.5f;

    const Rect label_area = take_left(area, area.width * kLabelFraction);
    draw_text(
        *ui.list, *ui.font, label_area.x, text_top, binding.label, label_color);

    const Rect reset_area = take_right(area, kResetWidth);
    const bool reset_hovered =
        rect_contains(reset_area, ui.pointer.x, ui.pointer.y);
    ui.list->push_rect(
        reset_area,
        reset_hovered ? theme::kControlHover : theme::kControl,
        theme::kControlRadius);
    draw_text_centered(*ui.list, *ui.font, reset_area, "R", theme::kTextDim);

    const Rect value_area = {
        reset_area.x - kValueWidth - theme::kRowGap,
        area.y,
        kValueWidth,
        area.height};
    char printed[32] = {};
    format_value(binding, *value, printed, sizeof(printed));
    draw_text_right(*ui.list, *ui.font, value_area, printed, label_color);

    const float track_start = label_area.x + label_area.width + theme::kRowGap;
    const Rect track = {
        track_start,
        area.y + (area.height - kTrackHeight) * 0.5f,
        value_area.x - theme::kRowGap - track_start,
        kTrackHeight};
    draw_track(ui, track, ratio_of(binding, *value));

    const Rect grab = {
        track.x, area.y, track.width, area.height};
    if (ui.pointer.pressed && rect_contains(grab, ui.pointer.x, ui.pointer.y)) {
        ui.active = &binding;
    }
    if (ui.active == &binding && ui.pointer.down) {
        const float updated = value_from_pointer(binding, track, ui.pointer.x);
        result.changed = updated != *value;
        *value = updated;
    }
    if (ui.active == &binding && !ui.pointer.down) {
        ui.active = nullptr;
    }
    if (ui.pointer.pressed && reset_hovered) {
        result.reset = true;
    }
    return result;
}

}
}
