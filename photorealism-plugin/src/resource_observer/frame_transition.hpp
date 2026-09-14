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
    bool reconstruct = false;
};

inline unsigned distance_between(unsigned first, unsigned second) {
    return first > second ? first - second : second - first;
}

class FrameTransition {
  public:
    void configure(
        void* output, unsigned expected_width, unsigned expected_height) {
        output_ = output;
        expected_width_ = expected_width;
        expected_height_ = expected_height;
    }

    TargetRole classify(void* texture, const TargetShape& shape) const {
        if (output_ == nullptr || texture == nullptr) {
            return TargetRole::Ignored;
        }
        if (texture == output_) {
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
        ++binds_;
        TransitionStep step;
        step.role = classify(texture, shape);
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
        if (step.role != TargetRole::Output) {
            return step;
        }
        if (pending_ != nullptr) {
            step.capture = pending_;
            pending_ = nullptr;
            ++transitions_;
            return step;
        }
        if (transitions_ == 0 || reconstructed_) {
            return step;
        }
        step.reconstruct = true;
        reconstructed_ = true;
        return step;
    }

    void* end_frame() {
        void* leftover = pending_;
        pending_ = nullptr;
        transitions_ = 0;
        internal_binds_ = 0;
        binds_ = 0;
        reconstructed_ = false;
        return leftover;
    }

    void* pending() const { return pending_; }
    unsigned transitions() const { return transitions_; }
    unsigned internal_binds() const { return internal_binds_; }
    unsigned binds() const { return binds_; }
    bool reconstructed() const { return reconstructed_; }

  private:
    void* output_ = nullptr;
    unsigned expected_width_ = 0;
    unsigned expected_height_ = 0;
    void* pending_ = nullptr;
    unsigned transitions_ = 0;
    unsigned internal_binds_ = 0;
    unsigned binds_ = 0;
    bool reconstructed_ = false;
};

}
}
