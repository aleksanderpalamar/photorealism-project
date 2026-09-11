#pragma once

#include "draw_list.hpp"

namespace photorealism {
namespace overlay {

class Layout {
  public:
    Layout(const Rect& area, float row_height, float gap);

    Rect row();
    Rect row(float height);
    void skip(float amount);

    float consumed() const { return cursor_ - area_.y; }
    float width() const { return area_.width; }

  private:
    Rect area_;
    float row_height_;
    float gap_;
    float cursor_;
};

Rect take_left(const Rect& area, float amount);
Rect take_right(const Rect& area, float amount);

}
}
