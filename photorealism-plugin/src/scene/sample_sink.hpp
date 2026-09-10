#pragma once

namespace photorealism {

class SceneSampleSink {
  public:
    virtual ~SceneSampleSink() = default;

    virtual void on_sample(
        const unsigned char* pixels,
        unsigned width,
        unsigned height,
        unsigned pitch,
        bool bgra) = 0;
};

}  // namespace photorealism
