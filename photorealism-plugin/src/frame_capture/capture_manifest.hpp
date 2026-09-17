#pragma once

#include "capture_records.hpp"

#include <string>
#include <vector>

namespace photorealism {
namespace frame_capture {

std::string manifest_json(
    const std::vector<BindRecord>& binds,
    const std::vector<SnapshotRecord>& snapshots,
    const std::vector<ConstantRecord>& constants,
    bool truncated);

}
}
