#pragma once

namespace photorealism {
namespace frame_capture {

enum class CaptureState {
    Idle,
    Armed,
    Recording,
    Saved,
    Failed,
};

enum class FrameAction {
    None,
    StartRecording,
    Flush,
};

class CaptureSchedule {
  public:
    bool request() {
        if (state_ == CaptureState::Armed || state_ == CaptureState::Recording) {
            return false;
        }
        state_ = CaptureState::Armed;
        return true;
    }

    FrameAction end_frame() {
        if (state_ == CaptureState::Armed) {
            state_ = CaptureState::Recording;
            return FrameAction::StartRecording;
        }
        return state_ == CaptureState::Recording ? FrameAction::Flush
                                                  : FrameAction::None;
    }

    void finish(bool saved) {
        state_ = saved ? CaptureState::Saved : CaptureState::Failed;
    }

    bool recording() const { return state_ == CaptureState::Recording; }
    CaptureState state() const { return state_; }

  private:
    CaptureState state_ = CaptureState::Idle;
};

}
}
