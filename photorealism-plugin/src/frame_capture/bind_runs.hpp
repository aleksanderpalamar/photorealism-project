#pragma once

#include "capture_records.hpp"

#include <vector>

namespace photorealism {
namespace frame_capture {

struct ReleasedTarget {
    TargetInfo target;
    unsigned first_bind;
    unsigned last_bind;
};

class BindRuns {
  public:
    void reset();
    unsigned identity_of(void* texture);
    unsigned bind(
        std::vector<TargetInfo>* targets,
        std::vector<ReleasedTarget>* released,
        std::vector<TargetInfo>* opened);
    void finish(std::vector<ReleasedTarget>* released);
    unsigned binds() const { return binds_; }

  private:
    struct Run {
        TargetInfo target;
        unsigned first_bind;
        unsigned last_bind;
    };

    std::vector<Run> open_;
    std::vector<void*> identities_;
    unsigned binds_ = 0;
};

}
}
