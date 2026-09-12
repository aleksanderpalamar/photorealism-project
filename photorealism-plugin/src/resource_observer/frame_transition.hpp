#pragma once

#include "../scene/formats.hpp"

namespace photorealism {
namespace observer {

constexpr unsigned kInternalSizeTolerance = 24;

struct TargetShape {
    unsigned width = 0;
    unsigned height = 0;
    unsigned format = 0;
    unsigned samples = 0;
};

enum class TargetRole {
    Ignored,
    Internal,
    Output,
};

struct TransitionStep {
    TargetRole role = TargetRole::Ignored;
    void* acquired = nullptr;
    void* released = nullptr;
    void* capture = nullptr;
};

inline unsigned distance_between(unsigned first, unsigned second) {
    return first > second ? first - second : second - first;
}

class FrameTransition {
  public:
    void configure(
        unsigned output_width,
        unsigned output_height,
        unsigned expected_width,
        unsigned expected_height) {
        output_width_ = output_width;
        output_height_ = output_height;
        expected_width_ = expected_width;
        expected_height_ = expected_height;
    }

    TargetRole classify(const TargetShape& shape) const {
        if (output_width_ == 0 || output_height_ == 0) {
            return TargetRole::Ignored;
        }
        if (shape.width == output_width_ && shape.height == output_height_) {
            return TargetRole::Output;
        }
        if (shape.samples != 1 || !scene_formats::is_readable(shape.format)) {
            return TargetRole::Ignored;
        }
        if (distance_between(shape.width, expected_width_) >
            kInternalSizeTolerance) {
            return TargetRole::Ignored;
        }
        if (distance_between(shape.height, expected_height_) >
            kInternalSizeTolerance) {
            return TargetRole::Ignored;
        }
        return TargetRole::Internal;
    }

    TransitionStep observe(void* texture, const TargetShape& shape) {
        TransitionStep step;
        step.role = classify(shape);
        if (step.role == TargetRole::Internal) {
            ++internal_binds_;
            if (texture == pending_) {
                return step;
            }
            step.acquired = texture;
            step.released = pending_;
            pending_ = texture;
            return step;
        }
        if (step.role != TargetRole::Output || pending_ == nullptr) {
            return step;
        }
        step.capture = pending_;
        pending_ = nullptr;
        ++transitions_;
        return step;
    }

    void* end_frame() {
        void* leftover = pending_;
        pending_ = nullptr;
        transitions_ = 0;
        internal_binds_ = 0;
        return leftover;
    }

    void* pending() const { return pending_; }
    unsigned transitions() const { return transitions_; }
    unsigned internal_binds() const { return internal_binds_; }

  private:
    unsigned output_width_ = 0;
    unsigned output_height_ = 0;
    unsigned expected_width_ = 0;
    unsigned expected_height_ = 0;
    void* pending_ = nullptr;
    unsigned transitions_ = 0;
    unsigned internal_binds_ = 0;
};

}
}
