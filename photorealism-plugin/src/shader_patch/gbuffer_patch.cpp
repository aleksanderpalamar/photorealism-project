#include "gbuffer_patch.hpp"

#include <cstdio>

namespace photorealism {
namespace shader_patch {
namespace {

constexpr char kSwizzleComponents[] = "xyzw";

std::string number(const char* prefix, std::uint32_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%s%u", prefix, value);
    return std::string(buffer);
}

bool output_matches(
    const Container& container,
    std::uint32_t index,
    ComponentType type) {
    for (const SignatureElement& element : container.outputs()) {
        if (element.register_index != index) {
            continue;
        }
        if (element.semantic_name.compare(0, 9, "SV_Target") != 0) {
            continue;
        }
        return element.component_type == type && element.mask == 0xF;
    }
    return false;
}

std::string coordinate_reference(const Operand& operand) {
    if (operand.type != OperandType::input || operand.indices.empty()) {
        return std::string();
    }
    std::string text = number("v", operand.indices[0].immediate);
    if (operand.selection == SelectionMode::swizzle) {
        text += '.';
        text.push_back(kSwizzleComponents[operand.swizzle[0]]);
        text.push_back(kSwizzleComponents[operand.swizzle[1]]);
    } else if (operand.selection == SelectionMode::select_one) {
        text += '.';
        text.push_back(kSwizzleComponents[operand.select_one]);
        text.push_back(kSwizzleComponents[operand.select_one]);
    } else {
        text += ".xy";
    }
    return text;
}

bool is_sample(std::uint32_t code) {
    return code == opcode::kSample || code == opcode::kSampleL ||
           code == opcode::kSampleB || code == opcode::kSampleD;
}

}  // namespace

GBufferInfo inspect_gbuffer_shader(
    const Container& container, const Program& program) {
    GBufferInfo info;

    if (!output_matches(container, 0, ComponentType::float32) ||
        !output_matches(container, 1, ComponentType::float32) ||
        !output_matches(container, 2, ComponentType::float32) ||
        !output_matches(container, 3, ComponentType::uint32)) {
        info.reason = "saidas nao formam o gbuffer defattr";
        return info;
    }
    for (const SignatureElement& element : container.outputs()) {
        if (element.semantic_name.compare(0, 9, "SV_Target") == 0 &&
            element.register_index > 3) {
            info.reason = "gbuffer com alvos extra";
            return info;
        }
    }

    for (const SignatureElement& element : container.inputs()) {
        if (element.semantic_name == "SV_Position" ||
            element.semantic_name == "SV_POSITION") {
            info.position_input = number("v", element.register_index);
        }
        if (element.semantic_name == "COLOR" && element.semantic_index == 0) {
            info.blend_input = number("v", element.register_index);
        }
    }
    if (info.position_input.empty()) {
        info.reason = "sem SV_Position na entrada";
        return info;
    }

    for (const Instruction& instruction : program.instructions) {
        if (!is_sample(instruction.opcode) || instruction.operands.size() < 4) {
            continue;
        }
        const Operand& texture = instruction.operands[2];
        const Operand& sampler = instruction.operands[3];
        if (texture.indices.empty() || sampler.indices.empty()) {
            continue;
        }
        const std::string uv = coordinate_reference(instruction.operands[1]);
        if (uv.empty()) {
            continue;
        }
        const std::string texture_name =
            number("t", texture.indices[0].immediate);
        const std::string sampler_name =
            number("s", sampler.indices[0].immediate);
        if (!info.has_albedo) {
            info.has_albedo = true;
            info.albedo_texture = texture_name;
            info.albedo_sampler = sampler_name;
            info.albedo_uv = uv;
        } else if (
            !info.has_second_albedo && texture_name != info.albedo_texture) {
            info.has_second_albedo = true;
            info.second_texture = texture_name;
            info.second_sampler = sampler_name;
            info.second_uv = uv;
        }
    }

    if (!info.has_albedo) {
        info.reason = "sem amostragem de albedo";
        return info;
    }

    for (const Instruction& instruction : program.instructions) {
        if (instruction.opcode != opcode::kMul ||
            instruction.operands.size() < 3) {
            continue;
        }
        for (std::size_t index = 1; index < 3; ++index) {
            const Operand& operand = instruction.operands[index];
            if (operand.type != OperandType::immediate32) {
                continue;
            }
            const float value = operand.immediate_float[0];
            if (value >= 1024.0f && value <= 65536.0f) {
                info.reflection_scale = value;
            }
        }
    }

    info.eligible = true;
    return info;
}

std::string build_injection_call(const GBufferInfo& info) {
    if (!info.eligible) {
        return std::string();
    }
    std::string call = "    photorealism_wet_surface(o0, o1, o2, o3, ";
    call += info.position_input;
    call += ", ";
    call += info.albedo_texture;
    call += ", ";
    call += info.albedo_sampler;
    call += ", ";
    call += info.albedo_uv;
    call += ", ";
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f", info.reflection_scale);
    call += buffer;
    call += ");";
    return call;
}

}
}
