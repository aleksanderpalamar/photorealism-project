#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace photorealism {
namespace shader_patch {

struct PatchStatistics {
    unsigned inspected = 0;
    unsigned patched = 0;
    unsigned from_cache = 0;
    unsigned rejected = 0;
    unsigned failed = 0;
};

bool is_enabled();
void set_enabled(bool enabled);

bool patch_pixel_shader(
    const void* bytecode, std::size_t size, std::vector<std::uint8_t>* patched);

PatchStatistics statistics();
void log_statistics(const char* phase);

}
}
