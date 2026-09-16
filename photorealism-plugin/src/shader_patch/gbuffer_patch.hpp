#pragma once

#include <string>

#include "dxbc.hpp"
#include "shex.hpp"

namespace photorealism {
namespace shader_patch {

struct GBufferInfo {
    bool eligible = false;
    bool has_albedo = false;
    bool has_second_albedo = false;
    std::string reason;
    std::string albedo_texture;
    std::string albedo_sampler;
    std::string albedo_uv;
    std::string second_texture;
    std::string second_sampler;
    std::string second_uv;
    std::string blend_input;
    std::string position_input;
    float reflection_scale = 4096.0f;
};

GBufferInfo inspect_gbuffer_shader(
    const Container& container, const Program& program);

std::string build_injection_call(const GBufferInfo& info);

}
}
