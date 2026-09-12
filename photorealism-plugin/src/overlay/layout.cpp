#include "layout.hpp"

namespace photorealism {
namespace overlay {

Layout::Layout(const Rect& area, float row_height, float gap)
    : area_(area), row_height_(row_height), gap_(gap), cursor_(area.y) {}

Rect Layout::row() {
    return row(row_height_);
}

Rect Layout::row(float height) {
    const Rect result = {area_.x, cursor_, area_.width, height};
    cursor_ += height + gap_;
    return result;
}

void Layout::skip(float amount) {
    cursor_ += amount;
}

Rect take_left(const Rect& area, float amount) {
    const float width = amount < area.width ? amount : area.width;
    const Rect result = {area.x, area.y, width, area.height};
    return result;
}

Rect take_right(const Rect& area, float amount) {
    const float width = amount < area.width ? amount : area.width;
    const Rect result = {
        area.x + area.width - width, area.y, width, area.height};
    return result;
}

}
}
