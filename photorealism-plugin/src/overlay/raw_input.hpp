#pragma once

#include <vector>
#include <windows.h>

namespace photorealism {
namespace overlay {

class RawInputBlock {
  public:
    void suspend();
    void restore();

    bool suspended() const { return suspended_; }

  private:
    std::vector<RAWINPUTDEVICE> saved_;
    bool suspended_ = false;
    bool reported_ = false;
};

RawInputBlock& raw_input_block();

}
}
