#pragma once

#include "capture_records.hpp"

#include <d3d11.h>

#include <vector>

namespace photorealism {
namespace frame_capture {

constexpr unsigned kMaximumConstantSnapshots = 4096;
constexpr unsigned kMaximumConstantBytes = 65536;

class ConstantSnapshots {
  public:
    void reset();
    void take(ID3D11DeviceContext* context, unsigned bind);
    unsigned write_all(ID3D11DeviceContext* context, const wchar_t* folder);

    const std::vector<ConstantRecord>& records() const { return records_; }

  private:
    void take_stage(
        ID3D11DeviceContext* context, unsigned bind, const char* stage,
        ID3D11Buffer* const* buffers);
    const char* copy_buffer(
        ID3D11DeviceContext* context, ID3D11Buffer* buffer, unsigned* bytes,
        ID3D11Buffer** staging);

    std::vector<ConstantRecord> records_;
    std::vector<ID3D11Buffer*> staging_;
};

}
}
