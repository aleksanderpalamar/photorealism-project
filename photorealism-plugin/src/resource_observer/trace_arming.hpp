#pragma once

namespace photorealism {
namespace observer {

constexpr unsigned kSceneBinds = 32;
constexpr unsigned kSettledSceneFrames = 300;

class TraceArming {
  public:
    bool end_frame(unsigned binds) {
        settled_ = binds >= kSceneBinds ? settled_ + 1 : 0;
        return settled_ >= kSettledSceneFrames;
    }

    unsigned settled() const { return settled_; }

  private:
    unsigned settled_ = 0;
};

}
}
