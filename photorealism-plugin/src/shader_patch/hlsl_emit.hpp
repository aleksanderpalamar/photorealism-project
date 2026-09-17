#pragma once

#include <string>

#include "dxbc.hpp"
#include "shex.hpp"

namespace photorealism {
namespace shader_patch {

struct EmitOptions {
    std::string entry_point = "main";
    std::string prologue;
    std::string injection;
};

struct EmitResult {
    std::string source;
    std::string failure;
    unsigned unsupported_opcode = 0;
    bool early_return = false;
};

bool emit_hlsl(
    const Container& container,
    const Program& program,
    const EmitOptions& options,
    EmitResult* result);

}
}
