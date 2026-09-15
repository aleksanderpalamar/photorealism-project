#pragma once

#include "bind_runs.hpp"
#include "capture_records.hpp"

#include <d3d11.h>

#include <vector>

namespace photorealism {
namespace frame_capture {

constexpr unsigned kMaximumSnapshots = 160;
constexpr unsigned long long kMaximumSnapshotBytes = 1536ull * 1024ull * 1024ull;

class StagingSnapshots {
  public:
    void reset();
    void take(ID3D11DeviceContext* context, const ReleasedTarget& released);
    unsigned write_all(ID3D11DeviceContext* context, const wchar_t* folder);

    const std::vector<SnapshotRecord>& records() const { return records_; }
    bool truncated() const { return truncated_; }

  private:
    struct Pending {
        ID3D11Texture2D* staging;
        unsigned subresource;
    };

    const char* copy_to_staging(
        ID3D11DeviceContext* context, const TargetInfo& target, Pending* pending);

    std::vector<SnapshotRecord> records_;
    std::vector<Pending> pending_;
    unsigned long long bytes_ = 0ull;
    bool truncated_ = false;
};

}
}
