#include "pass_trace.hpp"

#include "../runtime.hpp"

#include <cstdio>

namespace photorealism {
namespace observer {
namespace {

unsigned identity_of(std::vector<void*>* known, void* texture) {
    for (unsigned index = 0; index < known->size(); ++index) {
        if ((*known)[index] == texture) {
            return index + 1;
        }
    }
    known->push_back(texture);
    return static_cast<unsigned>(known->size());
}

const char* role_name(TargetRole role) {
    if (role == TargetRole::Internal) {
        return "interno";
    }
    return role == TargetRole::Output ? "saida" : "-";
}

}

void PassTrace::arm() {
    if (state_ != State::Idle) {
        return;
    }
    state_ = State::Waiting;
}

void PassTrace::record(TraceEntry entry) {
    if (!recording()) {
        return;
    }
    if (entries_.size() >= kTraceCapacity) {
        truncated_ = true;
        return;
    }
    entry.frame = frame_;
    entries_.push_back(entry);
}

void PassTrace::end_frame() {
    if (state_ == State::Waiting) {
        state_ = State::Recording;
        frame_ = 1;
        return;
    }
    if (state_ != State::Recording) {
        return;
    }
    if (frame_ < kTracedFrames) {
        ++frame_;
        return;
    }
    state_ = State::Ready;
}

bool PassTrace::take(std::vector<TraceEntry>* entries, bool* truncated) {
    if (state_ != State::Ready || entries == nullptr || truncated == nullptr) {
        return false;
    }
    entries->swap(entries_);
    *truncated = truncated_;
    state_ = State::Done;
    return true;
}

void log_trace(const std::vector<TraceEntry>& entries, bool truncated) {
    log_message(
        "FSR rastreio de %u quadros, depois de %u quadros seguidos de cena: %u "
        "passes%s. Cada linha e um OMSetRenderTargets do jogo; #N identifica a "
        "textura fisica; saida e o backbuffer.",
        kTracedFrames,
        kSettledSceneFrames,
        static_cast<unsigned>(entries.size()),
        truncated ? " (truncado)" : "");

    std::vector<void*> known;
    unsigned current_frame = 0;
    unsigned pass = 0;
    for (const TraceEntry& entry : entries) {
        pass = entry.frame == current_frame ? pass + 1 : 1;
        current_frame = entry.frame;

        char depth[24] = "-";
        if (entry.depth_width != 0) {
            std::snprintf(
                depth, sizeof(depth), "%ux%u", entry.depth_width,
                entry.depth_height);
        }
        char capture[24] = "";
        if (entry.captured != nullptr) {
            std::snprintf(
                capture, sizeof(capture), " captura=#%u",
                identity_of(&known, entry.captured));
        }
        log_message(
            "FSR rastreio q%u p%03u: rt=#%u %ux%u f%u n%u ds=%s %s%s%s.",
            entry.frame,
            pass,
            identity_of(&known, entry.texture),
            entry.shape.width,
            entry.shape.height,
            entry.shape.format,
            entry.targets,
            depth,
            role_name(entry.role),
            capture,
            entry.reconstruct ? " reconstrucao" : "");
    }
}

}
}
