#include "bind_runs.hpp"

namespace photorealism {
namespace frame_capture {
namespace {

bool contains(const std::vector<TargetInfo>& targets, const TargetKey& key) {
    for (const TargetInfo& target : targets) {
        if (same_target(target.key, key)) {
            return true;
        }
    }
    return false;
}

}

void BindRuns::reset() {
    open_.clear();
    identities_.clear();
    binds_ = 0;
}

unsigned BindRuns::identity_of(void* texture) {
    for (unsigned index = 0; index < identities_.size(); ++index) {
        if (identities_[index] == texture) {
            return index + 1;
        }
    }
    identities_.push_back(texture);
    return static_cast<unsigned>(identities_.size());
}

unsigned BindRuns::bind(
    std::vector<TargetInfo>* targets,
    std::vector<ReleasedTarget>* released,
    std::vector<TargetInfo>* opened) {
    ++binds_;
    std::vector<Run> kept;
    for (const Run& run : open_) {
        if (contains(*targets, run.target.key)) {
            kept.push_back(run);
            continue;
        }
        released->push_back({run.target, run.first_bind, run.last_bind});
    }
    for (TargetInfo& target : *targets) {
        target.identity = identity_of(target.key.texture);
        bool extended = false;
        for (Run& run : kept) {
            const bool same = same_target(run.target.key, target.key);
            run.last_bind = same ? binds_ : run.last_bind;
            extended = extended || same;
        }
        if (!extended) {
            kept.push_back({target, binds_, binds_});
            opened->push_back(target);
        }
    }
    open_.swap(kept);
    return binds_;
}

void BindRuns::finish(std::vector<ReleasedTarget>* released) {
    for (const Run& run : open_) {
        released->push_back({run.target, run.first_bind, run.last_bind});
    }
    open_.clear();
}

}
}
