#pragma once

#include <string>
#include <vector>

namespace photorealism {
namespace frame_capture {

struct TargetKey {
    void* texture = nullptr;
    unsigned mip = 0;
    unsigned slice = 0;
};

inline bool same_target(const TargetKey& left, const TargetKey& right) {
    return left.texture == right.texture && left.mip == right.mip &&
           left.slice == right.slice;
}

struct TargetInfo {
    TargetKey key;
    unsigned identity = 0;
    unsigned slot = 0;
    bool depth = false;
    unsigned width = 0;
    unsigned height = 0;
    unsigned texture_format = 0;
    unsigned view_format = 0;
    unsigned mip_levels = 1;
    unsigned array_size = 1;
    unsigned samples = 1;
};

struct BindRecord {
    unsigned index = 0;
    std::vector<TargetInfo> targets;
};

struct ConstantRecord {
    unsigned bind = 0;
    const char* stage = "";
    unsigned slot = 0;
    unsigned bytes = 0;
    std::string file;
    const char* failure = nullptr;
};

struct SnapshotRecord {
    TargetInfo target;
    unsigned first_bind = 0;
    unsigned last_bind = 0;
    std::string file;
    const char* failure = nullptr;
};

}
}
