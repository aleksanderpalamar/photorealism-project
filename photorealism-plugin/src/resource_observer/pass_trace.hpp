#pragma once

#include "frame_transition.hpp"
#include "trace_arming.hpp"

#include <vector>

namespace photorealism {
namespace observer {

constexpr unsigned kTraceCapacity = 512;
constexpr unsigned kTracedFrames = 2;

struct TraceEntry {
    unsigned frame = 0;
    void* texture = nullptr;
    TargetShape shape;
    unsigned targets = 0;
    unsigned depth_width = 0;
    unsigned depth_height = 0;
    TargetRole role = TargetRole::Ignored;
    void* captured = nullptr;
    bool reconstruct = false;
};

class PassTrace {
  public:
    void arm();
    bool recording() const { return state_ == State::Recording; }
    void record(TraceEntry entry);
    void end_frame();
    bool take(std::vector<TraceEntry>* entries, bool* truncated);

  private:
    enum class State {
        Idle,
        Waiting,
        Recording,
        Ready,
        Done,
    };

    State state_ = State::Idle;
    unsigned frame_ = 0;
    std::vector<TraceEntry> entries_;
    bool truncated_ = false;
};

void log_trace(const std::vector<TraceEntry>& entries, bool truncated);

}
}
