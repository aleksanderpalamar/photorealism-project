#pragma once

namespace photorealism {
namespace passfx {

constexpr unsigned kHdrViewFormat = 10;
constexpr unsigned kToneOutputViewFormat = 29;

struct BindShape {
    unsigned count = 0;
    unsigned view_format = 0;
    unsigned width = 0;
    unsigned height = 0;
};

class ToneStage {
  public:
    bool observe(const BindShape& shape) {
        const bool tone_output =
            previous_is_hdr_ && shape.count == 1 &&
            shape.view_format == kToneOutputViewFormat &&
            shape.width == previous_.width && shape.height == previous_.height;
        previous_is_hdr_ = shape.count == 1 && shape.view_format == kHdrViewFormat;
        previous_ = shape;
        return tone_output;
    }

    bool previous_is_hdr() const { return previous_is_hdr_; }
    void reset() { *this = ToneStage(); }

  private:
    BindShape previous_;
    bool previous_is_hdr_ = false;
};

}
}
