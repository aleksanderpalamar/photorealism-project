#include "hlsl_emit.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <set>

namespace photorealism {
namespace shader_patch {
namespace {

constexpr char kComponents[] = "xyzw";

enum class Domain { real, integer, unsigned_integer };

struct Context {
    const Container* container = nullptr;
    const Program* program = nullptr;
    std::map<std::uint32_t, std::uint32_t> buffer_sizes;
    std::set<std::uint32_t> samplers;
    std::set<std::uint32_t> comparison_samplers;
    std::map<std::uint32_t, std::uint32_t> resource_dimension;
    std::map<std::uint32_t, ComponentType> output_types;
    std::map<std::uint32_t, std::uint8_t> output_masks;
    unsigned highest_temp = 0;
};

std::string format(const char* pattern, ...) {
    char buffer[512];
    va_list arguments;
    va_start(arguments, pattern);
    std::vsnprintf(buffer, sizeof(buffer), pattern, arguments);
    va_end(arguments);
    return std::string(buffer);
}

int mask_component_count(std::uint8_t mask) {
    int count = 0;
    for (int index = 0; index < 4; ++index) {
        if ((mask & (1 << index)) != 0) {
            ++count;
        }
    }
    return count;
}

std::string mask_letters(std::uint8_t mask) {
    std::string result;
    for (int index = 0; index < 4; ++index) {
        if ((mask & (1 << index)) != 0) {
            result.push_back(kComponents[index]);
        }
    }
    return result;
}

const char* domain_scalar(Domain domain) {
    switch (domain) {
        case Domain::integer:
            return "int";
        case Domain::unsigned_integer:
            return "uint";
        default:
            return "float";
    }
}

std::string vector_type(Domain domain, int count) {
    if (count <= 1) {
        return domain_scalar(domain);
    }
    return format("%s%d", domain_scalar(domain), count);
}

std::string broadcast(Domain domain, int count, const std::string& value) {
    if (count <= 1) {
        return value;
    }
    std::string text = vector_type(domain, count) + "(";
    for (int index = 0; index < count; ++index) {
        if (index > 0) {
            text += ", ";
        }
        text += value;
    }
    return text + ")";
}

std::string register_name(const Operand& operand) {
    const std::uint32_t first =
        operand.indices.empty() ? 0 : operand.indices[0].immediate;
    switch (operand.type) {
        case OperandType::temp:
            return format("r%u", first);
        case OperandType::input:
            return format("v%u", first);
        case OperandType::output:
            return format("o%u", first);
        case OperandType::sampler:
            return format("s%u", first);
        case OperandType::resource:
            return format("t%u", first);
        case OperandType::constant_buffer: {
            if (operand.indices.size() < 2) {
                return format("cb%u", first);
            }
            const Index& element = operand.indices[1];
            if (element.relative) {
                return format(
                    "cb%u[%u + int(r%u.%c)]",
                    first,
                    element.immediate,
                    element.relative_register,
                    kComponents[element.relative_component]);
            }
            return format("cb%u[%u]", first, element.immediate);
        }
        case OperandType::indexable_temp: {
            if (operand.indices.size() < 2) {
                return format("x%u", first);
            }
            const Index& element = operand.indices[1];
            if (element.relative) {
                return format(
                    "x%u[%u + int(r%u.%c)]",
                    first,
                    element.immediate,
                    element.relative_register,
                    kComponents[element.relative_component]);
            }
            return format("x%u[%u]", first, element.immediate);
        }
        case OperandType::output_depth:
            return "oDepth";
        case OperandType::null:
            return "null";
        default:
            return format("unknown%u", static_cast<unsigned>(operand.type));
    }
}

std::string immediate_literal(
    const Operand& operand, std::uint8_t dest_mask, Domain domain) {
    const int count = mask_component_count(dest_mask);
    const bool scalar_source = operand.component_count <= 1;
    std::string values;
    for (int index = 0, emitted = 0; index < 4; ++index) {
        if ((dest_mask & (1 << index)) == 0) {
            continue;
        }
        const int source_index = scalar_source ? 0 : index;
        if (emitted++ > 0) {
            values += ", ";
        }
        if (domain == Domain::real) {
            const float value = operand.immediate_float[source_index];
            if (!std::isfinite(value)) {
                values += format(
                    "asfloat(%uu)", operand.immediate_raw[source_index]);
            } else {
                char buffer[64];
                std::snprintf(buffer, sizeof(buffer), "%.9g", value);
                std::string text(buffer);
                if (text.find('.') == std::string::npos &&
                    text.find('e') == std::string::npos) {
                    text += ".0";
                }
                values += text;
            }
        } else if (domain == Domain::unsigned_integer) {
            values += format("%uu", operand.immediate_raw[source_index]);
        } else {
            values += format(
                "%d",
                static_cast<std::int32_t>(operand.immediate_raw[source_index]));
        }
    }
    if (count <= 1) {
        return values;
    }
    return vector_type(domain, count) + "(" + values + ")";
}

Domain natural_domain(const Context& context, const Operand& operand) {
    if (operand.type != OperandType::output) {
        return Domain::real;
    }
    const std::uint32_t index =
        operand.indices.empty() ? 0 : operand.indices[0].immediate;
    const auto found = context.output_types.find(index);
    if (found == context.output_types.end()) {
        return Domain::real;
    }
    if (found->second == ComponentType::uint32) {
        return Domain::unsigned_integer;
    }
    if (found->second == ComponentType::sint32) {
        return Domain::integer;
    }
    return Domain::real;
}

std::string source_expression(
    const Context& context,
    const Operand& operand,
    std::uint8_t dest_mask,
    Domain domain) {
    if (operand.type == OperandType::immediate32) {
        return immediate_literal(operand, dest_mask, domain);
    }

    std::string swizzle;
    if (operand.selection == SelectionMode::swizzle) {
        for (int index = 0; index < 4; ++index) {
            if ((dest_mask & (1 << index)) != 0) {
                swizzle.push_back(kComponents[operand.swizzle[index]]);
            }
        }
    } else if (operand.selection == SelectionMode::select_one) {
        const int count = mask_component_count(dest_mask);
        for (int index = 0; index < count; ++index) {
            swizzle.push_back(kComponents[operand.select_one]);
        }
    } else if (operand.selection == SelectionMode::mask) {
        swizzle = mask_letters(operand.mask);
    }

    std::string text = register_name(operand);
    if (!swizzle.empty()) {
        text += "." + swizzle;
    }

    const Domain stored = natural_domain(context, operand);
    if (domain != stored) {
        if (domain == Domain::real) {
            text = "asfloat(" + text + ")";
        } else if (domain == Domain::integer) {
            text = "asint(" + text + ")";
        } else {
            text = "asuint(" + text + ")";
        }
    }

    if (operand.modifier == Modifier::absolute ||
        operand.modifier == Modifier::absolute_negate) {
        text = "abs(" + text + ")";
    }
    if (operand.modifier == Modifier::negate ||
        operand.modifier == Modifier::absolute_negate) {
        text = "-" + text;
    }
    return text;
}

std::string destination_expression(const Operand& operand) {
    std::string text = register_name(operand);
    if (operand.selection == SelectionMode::mask && operand.mask != 0 &&
        operand.mask != 0xF) {
        text += "." + mask_letters(operand.mask);
    } else if (operand.selection == SelectionMode::mask && operand.mask == 0xF) {
        text += ".xyzw";
    }
    return text;
}

std::string wrap_for_destination(
    const Context& context,
    const Operand& destination,
    const std::string& value,
    Domain value_domain) {
    const Domain stored = natural_domain(context, destination);
    if (stored == value_domain) {
        return value;
    }
    if (stored == Domain::real) {
        return "asfloat(" + value + ")";
    }
    if (stored == Domain::integer) {
        return "asint(" + value + ")";
    }
    return "asuint(" + value + ")";
}

std::string saturate_if(bool saturate, const std::string& value) {
    return saturate ? "saturate(" + value + ")" : value;
}

bool test_non_zero(const Instruction& instruction) {
    return ((instruction.control >> 7) & 1u) != 0;
}

int resource_coordinate_count(const Context& context, std::uint32_t slot) {
    const auto found = context.resource_dimension.find(slot);
    const std::uint32_t dimension =
        found == context.resource_dimension.end() ? 3u : found->second;
    switch (dimension) {
        case 2:
            return 1;
        case 5:
        case 6:
            return 3;
        case 7:
            return 2;
        case 8:
            return 3;
        default:
            return 2;
    }
}

std::string coordinate_expression(
    const Context& context, const Operand& operand, int count) {
    std::uint8_t mask = 0;
    for (int index = 0; index < count; ++index) {
        mask |= static_cast<std::uint8_t>(1 << index);
    }
    return source_expression(context, operand, mask, Domain::real);
}

const char* resource_hlsl_type(const Context& context, std::uint32_t slot) {
    const auto found = context.resource_dimension.find(slot);
    const std::uint32_t dimension =
        found == context.resource_dimension.end() ? 3u : found->second;
    switch (dimension) {
        case 2:
            return "Texture1D<float4>";
        case 5:
            return "Texture3D<float4>";
        case 6:
            return "TextureCube<float4>";
        case 7:
            return "Texture1DArray<float4>";
        case 8:
            return "Texture2DArray<float4>";
        case 4:
            return "Texture2DMS<float4>";
        default:
            return "Texture2D<float4>";
    }
}

}  // namespace

bool emit_hlsl(
    const Container& container,
    const Program& program,
    const EmitOptions& options,
    EmitResult* result) {
    if (result == nullptr) {
        return false;
    }
    result->source.clear();
    result->failure.clear();
    result->unsupported_opcode = 0;

    Context context;
    context.container = &container;
    context.program = &program;

    for (const SignatureElement& element : container.outputs()) {
        if (element.semantic_name.compare(0, 9, "SV_Target") != 0) {
            continue;
        }
        context.output_types[element.register_index] = element.component_type;
        context.output_masks[element.register_index] |= element.mask;
    }

    for (const Instruction& instruction : program.instructions) {
        if (instruction.opcode == opcode::kDclResource &&
            !instruction.operands.empty() &&
            !instruction.operands[0].indices.empty()) {
            context.resource_dimension[instruction.operands[0].indices[0]
                                           .immediate] =
                instruction.control & 0x1Fu;
        }
        if (instruction.opcode == opcode::kDclSampler &&
            !instruction.operands.empty() &&
            !instruction.operands[0].indices.empty()) {
            const std::uint32_t slot =
                instruction.operands[0].indices[0].immediate;
            context.samplers.insert(slot);
            if ((instruction.control & 0xFu) == 1u) {
                context.comparison_samplers.insert(slot);
            }
        }
        if ((instruction.opcode == opcode::kSampleC ||
             instruction.opcode == opcode::kSampleCLz) &&
            instruction.operands.size() >= 4 &&
            !instruction.operands[3].indices.empty()) {
            context.comparison_samplers.insert(
                instruction.operands[3].indices[0].immediate);
        }
        if (instruction.opcode == opcode::kDclConstantBuffer &&
            !instruction.operands.empty() &&
            instruction.operands[0].indices.size() >= 2) {
            const std::uint32_t slot =
                instruction.operands[0].indices[0].immediate;
            const std::uint32_t size =
                instruction.operands[0].indices[1].immediate;
            std::uint32_t& stored = context.buffer_sizes[slot];
            if (size > stored) {
                stored = size;
            }
        }
        for (const Operand& operand : instruction.operands) {
            if (operand.type == OperandType::temp && !operand.indices.empty()) {
                const std::uint32_t index = operand.indices[0].immediate;
                if (index + 1 > context.highest_temp) {
                    context.highest_temp = index + 1;
                }
            }
            for (const Index& index : operand.indices) {
                if (index.relative &&
                    index.relative_register + 1 > context.highest_temp) {
                    context.highest_temp = index.relative_register + 1;
                }
            }
        }
    }

    std::string text;
    text.reserve(16384);

    for (const auto& entry : context.samplers) {
        text += format(
            "%s s%u : register(s%u);\n",
            context.comparison_samplers.count(entry) != 0
                ? "SamplerComparisonState"
                : "SamplerState",
            entry,
            entry);
    }
    for (const auto& entry : context.resource_dimension) {
        text += format(
            "%s t%u : register(t%u);\n",
            resource_hlsl_type(context, entry.first),
            entry.first,
            entry.first);
    }
    for (const auto& entry : context.buffer_sizes) {
        text += format(
            "cbuffer photorealism_cb%u : register(b%u) { float4 cb%u[%u]; }\n",
            entry.first,
            entry.first,
            entry.first,
            entry.second == 0 ? 1u : entry.second);
    }
    text += "\n";
    if (!options.prologue.empty()) {
        text += options.prologue;
        text += "\n";
    }

    text += format("void %s(\n", options.entry_point.c_str());
    bool first_parameter = true;
    std::map<std::uint32_t, unsigned> register_share;
    for (const SignatureElement& element : container.inputs()) {
        ++register_share[element.register_index];
    }
    std::map<std::uint32_t, unsigned> register_seen;
    std::string input_assembly;
    for (const SignatureElement& element : container.inputs()) {
        const int count = mask_component_count(element.mask);
        const char* type_name = component_type_name(element.component_type);
        std::string semantic = element.semantic_name;
        if (semantic == "SV_Position" || semantic == "SV_POSITION") {
            semantic = "SV_Position";
        } else {
            semantic += std::to_string(element.semantic_index);
        }
        const bool packed = register_share[element.register_index] > 1;
        const unsigned slot = register_seen[element.register_index]++;
        const std::string name =
            packed ? format("v%u_%u", element.register_index, slot)
                   : format("v%u", element.register_index);
        if (!first_parameter) {
            text += ",\n";
        }
        first_parameter = false;
        text += format(
            "    %s%d %s : %s",
            type_name,
            count < 1 ? 1 : count,
            name.c_str(),
            semantic.c_str());
        if (packed) {
            input_assembly += format(
                "    v%u.%s = %s;\n",
                element.register_index,
                mask_letters(element.mask).c_str(),
                name.c_str());
        }
    }
    std::set<std::uint32_t> emitted_outputs;
    for (const SignatureElement& element : container.outputs()) {
        if (element.semantic_name.compare(0, 9, "SV_Target") != 0) {
            continue;
        }
        if (!emitted_outputs.insert(element.register_index).second) {
            continue;
        }
        if (!first_parameter) {
            text += ",\n";
        }
        first_parameter = false;
        text += format(
            "    out %s4 o%u : SV_Target%u",
            component_type_name(element.component_type),
            element.register_index,
            element.register_index);
    }
    text += ")\n{\n";

    for (const auto& entry : context.output_types) {
        text += format("    o%u = 0;\n", entry.first);
    }
    for (unsigned index = 0; index < context.highest_temp; ++index) {
        text += format("    float4 r%u = 0;\n", index);
    }
    for (const auto& entry : program.indexable_temps) {
        text += format(
            "    float4 x%u[%u];\n",
            entry.first,
            entry.second == 0 ? 1u : entry.second);
    }
    if (!input_assembly.empty()) {
        for (const auto& entry : register_share) {
            if (entry.second > 1) {
                text += format("    float4 v%u = 0;\n", entry.first);
            }
        }
        text += input_assembly;
    }
    text += "\n";

    std::size_t last_executable = 0;
    for (std::size_t index = 0; index < program.instructions.size(); ++index) {
        const std::uint32_t code = program.instructions[index].opcode;
        if (code >= opcode::kDclResource && code <= opcode::kDclGlobalFlags) {
            continue;
        }
        last_executable = index;
    }

    for (std::size_t position = 0; position < program.instructions.size();
         ++position) {
        const Instruction& instruction = program.instructions[position];
        const std::vector<Operand>& ops = instruction.operands;
        const std::uint32_t code = instruction.opcode;
        if (code >= opcode::kDclResource && code <= opcode::kDclGlobalFlags) {
            continue;
        }
        if (code == opcode::kDclGlobalFlags || code == opcode::kNop) {
            continue;
        }
        if (code == opcode::kRet) {
            if (position != last_executable) {
                result->early_return = true;
                text += "    return;\n";
            }
            continue;
        }

        auto dest = [&](void) -> const Operand& { return ops[0]; };
        auto dest_mask = [&](void) -> std::uint8_t {
            return ops[0].selection == SelectionMode::mask && ops[0].mask != 0
                       ? ops[0].mask
                       : 0xF;
        };

        auto binary = [&](const char* op, Domain domain) {
            const std::string a =
                source_expression(context, ops[1], dest_mask(), domain);
            const std::string b =
                source_expression(context, ops[2], dest_mask(), domain);
            const std::string value = "(" + a + " " + op + " " + b + ")";
            text += format(
                "    %s = %s;\n",
                destination_expression(dest()).c_str(),
                saturate_if(
                    instruction.saturate,
                    wrap_for_destination(context, dest(), value, domain))
                    .c_str());
        };

        auto call1 = [&](const char* fn, Domain domain) {
            const std::string a =
                source_expression(context, ops[1], dest_mask(), domain);
            const std::string value = std::string(fn) + "(" + a + ")";
            text += format(
                "    %s = %s;\n",
                destination_expression(dest()).c_str(),
                saturate_if(
                    instruction.saturate,
                    wrap_for_destination(context, dest(), value, domain))
                    .c_str());
        };

        auto call2 = [&](const char* fn, Domain domain) {
            const std::string a =
                source_expression(context, ops[1], dest_mask(), domain);
            const std::string b =
                source_expression(context, ops[2], dest_mask(), domain);
            const std::string value =
                std::string(fn) + "(" + a + ", " + b + ")";
            text += format(
                "    %s = %s;\n",
                destination_expression(dest()).c_str(),
                saturate_if(
                    instruction.saturate,
                    wrap_for_destination(context, dest(), value, domain))
                    .c_str());
        };

        auto compare = [&](const char* op, Domain domain) {
            const std::string a =
                source_expression(context, ops[1], dest_mask(), domain);
            const std::string b =
                source_expression(context, ops[2], dest_mask(), domain);
            const int count = mask_component_count(dest_mask());
            const std::string fill_true =
                broadcast(Domain::unsigned_integer, count, "0xFFFFFFFFu");
            const std::string fill_false =
                broadcast(Domain::unsigned_integer, count, "0u");
            const std::string expression = "(" + a + " " + op + " " + b +
                                           " ? " + fill_true + " : " +
                                           fill_false + ")";
            text += format(
                "    %s = %s;\n",
                destination_expression(dest()).c_str(),
                wrap_for_destination(
                    context, dest(), expression, Domain::unsigned_integer)
                    .c_str());
        };

        auto dot = [&](int count) {
            std::uint8_t mask = 0;
            for (int index = 0; index < count; ++index) {
                mask |= static_cast<std::uint8_t>(1 << index);
            }
            const std::string a =
                source_expression(context, ops[1], mask, Domain::real);
            const std::string b =
                source_expression(context, ops[2], mask, Domain::real);
            const std::string value = "dot(" + a + ", " + b + ")";
            text += format(
                "    %s = %s;\n",
                destination_expression(dest()).c_str(),
                saturate_if(
                    instruction.saturate,
                    wrap_for_destination(context, dest(), value, Domain::real))
                    .c_str());
        };

        switch (code) {
            case opcode::kMov: {
                const Domain domain = natural_domain(context, ops[0]);
                const std::string a =
                    source_expression(context, ops[1], dest_mask(), domain);
                text += format(
                    "    %s = %s;\n",
                    destination_expression(dest()).c_str(),
                    saturate_if(instruction.saturate, a).c_str());
                break;
            }
            case opcode::kMovc: {
                const Domain domain = natural_domain(context, ops[0]);
                const std::string condition = source_expression(
                    context, ops[1], dest_mask(), Domain::unsigned_integer);
                const std::string a =
                    source_expression(context, ops[2], dest_mask(), domain);
                const std::string b =
                    source_expression(context, ops[3], dest_mask(), domain);
                text += format(
                    "    %s = (%s != 0 ? %s : %s);\n",
                    destination_expression(dest()).c_str(),
                    condition.c_str(),
                    a.c_str(),
                    b.c_str());
                break;
            }
            case opcode::kAdd:
                binary("+", Domain::real);
                break;
            case opcode::kMul:
                binary("*", Domain::real);
                break;
            case opcode::kDiv:
                binary("/", Domain::real);
                break;
            case opcode::kIadd:
                binary("+", Domain::integer);
                break;
            case opcode::kAnd:
                binary("&", Domain::unsigned_integer);
                break;
            case opcode::kOr:
                binary("|", Domain::unsigned_integer);
                break;
            case opcode::kXor:
                binary("^", Domain::unsigned_integer);
                break;
            case opcode::kIshl:
                binary("<<", Domain::integer);
                break;
            case opcode::kIshr:
                binary(">>", Domain::integer);
                break;
            case opcode::kUshr:
                binary(">>", Domain::unsigned_integer);
                break;
            case opcode::kImul:
            case opcode::kUmul: {
                const Domain domain = code == opcode::kImul
                                          ? Domain::integer
                                          : Domain::unsigned_integer;
                const std::string a =
                    source_expression(context, ops[2], dest_mask(), domain);
                const std::string b =
                    source_expression(context, ops[3], dest_mask(), domain);
                const std::string value = "(" + a + " * " + b + ")";
                text += format(
                    "    %s = %s;\n",
                    destination_expression(ops[1]).c_str(),
                    wrap_for_destination(context, ops[1], value, domain)
                        .c_str());
                break;
            }
            case opcode::kUdiv: {
                const std::string a = source_expression(
                    context, ops[2], dest_mask(), Domain::unsigned_integer);
                const std::string b = source_expression(
                    context, ops[3], dest_mask(), Domain::unsigned_integer);
                const std::string value = "(" + a + " / " + b + ")";
                text += format(
                    "    %s = %s;\n",
                    destination_expression(ops[0]).c_str(),
                    wrap_for_destination(
                        context, ops[0], value, Domain::unsigned_integer)
                        .c_str());
                break;
            }
            case opcode::kMad:
            case opcode::kImad:
            case opcode::kUmad: {
                const Domain domain = code == opcode::kMad
                                          ? Domain::real
                                          : (code == opcode::kImad
                                                 ? Domain::integer
                                                 : Domain::unsigned_integer);
                const std::string a =
                    source_expression(context, ops[1], dest_mask(), domain);
                const std::string b =
                    source_expression(context, ops[2], dest_mask(), domain);
                const std::string c =
                    source_expression(context, ops[3], dest_mask(), domain);
                const std::string value =
                    "(" + a + " * " + b + " + " + c + ")";
                text += format(
                    "    %s = %s;\n",
                    destination_expression(dest()).c_str(),
                    saturate_if(
                        instruction.saturate,
                        wrap_for_destination(context, dest(), value, domain))
                        .c_str());
                break;
            }
            case opcode::kMin:
                call2("min", Domain::real);
                break;
            case opcode::kMax:
                call2("max", Domain::real);
                break;
            case opcode::kImin:
                call2("min", Domain::integer);
                break;
            case opcode::kImax:
                call2("max", Domain::integer);
                break;
            case opcode::kUmin:
                call2("min", Domain::unsigned_integer);
                break;
            case opcode::kUmax:
                call2("max", Domain::unsigned_integer);
                break;
            case opcode::kExp:
                call1("exp2", Domain::real);
                break;
            case opcode::kLog:
                call1("log2", Domain::real);
                break;
            case opcode::kRsq:
                call1("rsqrt", Domain::real);
                break;
            case opcode::kSqrt:
                call1("sqrt", Domain::real);
                break;
            case opcode::kRcp:
                call1("rcp", Domain::real);
                break;
            case opcode::kFrc:
                call1("frac", Domain::real);
                break;
            case opcode::kRoundNe:
                call1("round", Domain::real);
                break;
            case opcode::kRoundNi:
                call1("floor", Domain::real);
                break;
            case opcode::kRoundPi:
                call1("ceil", Domain::real);
                break;
            case opcode::kRoundZ:
                call1("trunc", Domain::real);
                break;
            case opcode::kNot:
                call1("~", Domain::unsigned_integer);
                break;
            case opcode::kIneg: {
                const std::string a = source_expression(
                    context, ops[1], dest_mask(), Domain::integer);
                text += format(
                    "    %s = %s;\n",
                    destination_expression(dest()).c_str(),
                    wrap_for_destination(
                        context, dest(), "(-" + a + ")", Domain::integer)
                        .c_str());
                break;
            }
            case opcode::kCountBits:
                call1("countbits", Domain::unsigned_integer);
                break;
            case opcode::kBfRev:
                call1("reversebits", Domain::unsigned_integer);
                break;
            case opcode::kFtoi: {
                const std::string a =
                    source_expression(context, ops[1], dest_mask(), Domain::real);
                const int count = mask_component_count(dest_mask());
                const std::string value =
                    vector_type(Domain::integer, count) + "(" + a + ")";
                text += format(
                    "    %s = %s;\n",
                    destination_expression(dest()).c_str(),
                    wrap_for_destination(context, dest(), value, Domain::integer)
                        .c_str());
                break;
            }
            case opcode::kFtou: {
                const std::string a =
                    source_expression(context, ops[1], dest_mask(), Domain::real);
                const int count = mask_component_count(dest_mask());
                const std::string value =
                    vector_type(Domain::unsigned_integer, count) + "(" + a + ")";
                text += format(
                    "    %s = %s;\n",
                    destination_expression(dest()).c_str(),
                    wrap_for_destination(
                        context, dest(), value, Domain::unsigned_integer)
                        .c_str());
                break;
            }
            case opcode::kItof: {
                const std::string a = source_expression(
                    context, ops[1], dest_mask(), Domain::integer);
                const int count = mask_component_count(dest_mask());
                const std::string value =
                    vector_type(Domain::real, count) + "(" + a + ")";
                text += format(
                    "    %s = %s;\n",
                    destination_expression(dest()).c_str(),
                    saturate_if(
                        instruction.saturate,
                        wrap_for_destination(
                            context, dest(), value, Domain::real))
                        .c_str());
                break;
            }
            case opcode::kUtof: {
                const std::string a = source_expression(
                    context, ops[1], dest_mask(), Domain::unsigned_integer);
                const int count = mask_component_count(dest_mask());
                const std::string value =
                    vector_type(Domain::real, count) + "(" + a + ")";
                text += format(
                    "    %s = %s;\n",
                    destination_expression(dest()).c_str(),
                    saturate_if(
                        instruction.saturate,
                        wrap_for_destination(
                            context, dest(), value, Domain::real))
                        .c_str());
                break;
            }
            case opcode::kEq:
                compare("==", Domain::real);
                break;
            case opcode::kNe:
                compare("!=", Domain::real);
                break;
            case opcode::kLt:
                compare("<", Domain::real);
                break;
            case opcode::kGe:
                compare(">=", Domain::real);
                break;
            case opcode::kIeq:
                compare("==", Domain::integer);
                break;
            case opcode::kIne:
                compare("!=", Domain::integer);
                break;
            case opcode::kIlt:
                compare("<", Domain::integer);
                break;
            case opcode::kIge:
                compare(">=", Domain::integer);
                break;
            case opcode::kUlt:
                compare("<", Domain::unsigned_integer);
                break;
            case opcode::kUge:
                compare(">=", Domain::unsigned_integer);
                break;
            case opcode::kDp2:
                dot(2);
                break;
            case opcode::kDp3:
                dot(3);
                break;
            case opcode::kDp4:
                dot(4);
                break;
            case opcode::kDerivRtx:
            case opcode::kDerivRtxCoarse:
            case opcode::kDerivRtxFine:
                call1("ddx", Domain::real);
                break;
            case opcode::kDerivRty:
            case opcode::kDerivRtyCoarse:
            case opcode::kDerivRtyFine:
                call1("ddy", Domain::real);
                break;
            case opcode::kSinCos: {
                const std::uint8_t sin_mask =
                    ops[0].selection == SelectionMode::mask ? ops[0].mask : 0;
                const std::uint8_t cos_mask =
                    ops[1].selection == SelectionMode::mask ? ops[1].mask : 0;
                if (sin_mask != 0) {
                    const std::string a = source_expression(
                        context, ops[2], sin_mask, Domain::real);
                    text += format(
                        "    %s = sin(%s);\n",
                        destination_expression(ops[0]).c_str(),
                        a.c_str());
                }
                if (cos_mask != 0) {
                    const std::string a = source_expression(
                        context, ops[2], cos_mask, Domain::real);
                    text += format(
                        "    %s = cos(%s);\n",
                        destination_expression(ops[1]).c_str(),
                        a.c_str());
                }
                break;
            }
            case opcode::kUbfe:
            case opcode::kIbfe: {
                const Domain domain = code == opcode::kUbfe
                                          ? Domain::unsigned_integer
                                          : Domain::integer;
                const std::string width = source_expression(
                    context, ops[1], dest_mask(), Domain::unsigned_integer);
                const std::string offset = source_expression(
                    context, ops[2], dest_mask(), Domain::unsigned_integer);
                const std::string value =
                    source_expression(context, ops[3], dest_mask(), domain);
                const std::string expression = "((" + value + " >> (" + offset +
                                               " & 31u)) & ((1u << (" + width +
                                               " & 31u)) - 1u))";
                text += format(
                    "    %s = %s;\n",
                    destination_expression(dest()).c_str(),
                    wrap_for_destination(
                        context, dest(), expression, Domain::unsigned_integer)
                        .c_str());
                break;
            }
            case opcode::kBfi: {
                const std::string width = source_expression(
                    context, ops[1], dest_mask(), Domain::unsigned_integer);
                const std::string offset = source_expression(
                    context, ops[2], dest_mask(), Domain::unsigned_integer);
                const std::string insert = source_expression(
                    context, ops[3], dest_mask(), Domain::unsigned_integer);
                const std::string base = source_expression(
                    context, ops[4], dest_mask(), Domain::unsigned_integer);
                const std::string mask_expression = "(((1u << (" + width +
                                                    " & 31u)) - 1u) << (" +
                                                    offset + " & 31u))";
                const std::string expression = "(((" + insert + " << (" +
                                               offset + " & 31u)) & " +
                                               mask_expression + ") | (" + base +
                                               " & ~" + mask_expression + "))";
                text += format(
                    "    %s = %s;\n",
                    destination_expression(dest()).c_str(),
                    wrap_for_destination(
                        context, dest(), expression, Domain::unsigned_integer)
                        .c_str());
                break;
            }
            case opcode::kF32ToF16:
                call1("f32tof16", Domain::real);
                break;
            case opcode::kF16ToF32:
                call1("f16tof32", Domain::unsigned_integer);
                break;
            case opcode::kDiscard: {
                const std::string a = source_expression(
                    context, ops[0], 0x1, Domain::unsigned_integer);
                text += format(
                    "    if (%s %s 0) discard;\n",
                    a.c_str(),
                    test_non_zero(instruction) ? "!=" : "==");
                break;
            }
            case opcode::kIf: {
                const std::string a = source_expression(
                    context, ops[0], 0x1, Domain::unsigned_integer);
                text += format(
                    "    if (%s %s 0) {\n",
                    a.c_str(),
                    test_non_zero(instruction) ? "!=" : "==");
                break;
            }
            case opcode::kElse:
                text += "    } else {\n";
                break;
            case opcode::kEndif:
            case opcode::kEndloop:
            case opcode::kEndswitch:
                text += "    }\n";
                break;
            case opcode::kLoop:
                text += "    [loop] while (true) {\n";
                break;
            case opcode::kBreak:
                text += "    break;\n";
                break;
            case opcode::kContinue:
                text += "    continue;\n";
                break;
            case opcode::kBreakc:
            case opcode::kContinuec: {
                const std::string a = source_expression(
                    context, ops[0], 0x1, Domain::unsigned_integer);
                text += format(
                    "    if (%s %s 0) %s;\n",
                    a.c_str(),
                    test_non_zero(instruction) ? "!=" : "==",
                    code == opcode::kBreakc ? "break" : "continue");
                break;
            }
            case opcode::kSwitch: {
                const std::string a = source_expression(
                    context, ops[0], 0x1, Domain::integer);
                text += format("    switch (%s) {\n", a.c_str());
                break;
            }
            case opcode::kCase: {
                const std::string a = source_expression(
                    context, ops[0], 0x1, Domain::integer);
                text += format("    case %s:\n", a.c_str());
                break;
            }
            case opcode::kDefault:
                text += "    default:\n";
                break;
            case opcode::kRetc: {
                const std::string a = source_expression(
                    context, ops[0], 0x1, Domain::unsigned_integer);
                result->early_return = true;
                text += format(
                    "    if (%s %s 0) return;\n",
                    a.c_str(),
                    test_non_zero(instruction) ? "!=" : "==");
                break;
            }
            case opcode::kSample:
            case opcode::kSampleL:
            case opcode::kSampleB:
            case opcode::kSampleD:
            case opcode::kSampleC:
            case opcode::kSampleCLz: {
                if (ops.size() < 4) {
                    break;
                }
                const Operand& texture = ops[2];
                const Operand& sampler = ops[3];
                const std::uint32_t slot =
                    texture.indices.empty() ? 0 : texture.indices[0].immediate;
                const int coordinates = resource_coordinate_count(context, slot);
                const std::string uv =
                    coordinate_expression(context, ops[1], coordinates);
                std::string call;
                if (code == opcode::kSample) {
                    call = register_name(texture) + ".Sample(" +
                           register_name(sampler) + ", " + uv + ")";
                } else if (code == opcode::kSampleL) {
                    call = register_name(texture) + ".SampleLevel(" +
                           register_name(sampler) + ", " + uv + ", " +
                           source_expression(context, ops[4], 0x1, Domain::real) +
                           ")";
                } else if (code == opcode::kSampleB) {
                    call = register_name(texture) + ".SampleBias(" +
                           register_name(sampler) + ", " + uv + ", " +
                           source_expression(context, ops[4], 0x1, Domain::real) +
                           ")";
                } else if (code == opcode::kSampleD) {
                    call = register_name(texture) + ".SampleGrad(" +
                           register_name(sampler) + ", " + uv + ", " +
                           coordinate_expression(context, ops[4], coordinates) +
                           ", " +
                           coordinate_expression(context, ops[5], coordinates) +
                           ")";
                } else if (code == opcode::kSampleC) {
                    call = register_name(texture) + ".SampleCmp(" +
                           register_name(sampler) + ", " + uv + ", " +
                           source_expression(context, ops[4], 0x1, Domain::real) +
                           ")";
                } else {
                    call = register_name(texture) + ".SampleCmpLevelZero(" +
                           register_name(sampler) + ", " + uv + ", " +
                           source_expression(context, ops[4], 0x1, Domain::real) +
                           ")";
                }

                std::string swizzle;
                if (texture.selection == SelectionMode::swizzle) {
                    for (int index = 0; index < 4; ++index) {
                        if ((dest_mask() & (1 << index)) != 0) {
                            swizzle.push_back(
                                kComponents[texture.swizzle[index]]);
                        }
                    }
                }
                const bool comparison =
                    code == opcode::kSampleC || code == opcode::kSampleCLz;
                if (comparison) {
                    call = broadcast(
                        Domain::real, mask_component_count(dest_mask()), call);
                } else if (!swizzle.empty()) {
                    call += "." + swizzle;
                }
                text += format(
                    "    %s = %s;\n",
                    destination_expression(dest()).c_str(),
                    saturate_if(
                        instruction.saturate,
                        wrap_for_destination(
                            context, dest(), call, Domain::real))
                        .c_str());
                break;
            }
            case opcode::kLd: {
                if (ops.size() < 3) {
                    break;
                }
                const Operand& texture = ops[2];
                const std::uint32_t slot =
                    texture.indices.empty() ? 0 : texture.indices[0].immediate;
                const int coordinates =
                    resource_coordinate_count(context, slot) + 1;
                std::uint8_t mask = 0;
                for (int index = 0; index < coordinates; ++index) {
                    mask |= static_cast<std::uint8_t>(1 << index);
                }
                const std::string location = source_expression(
                    context, ops[1], mask, Domain::integer);
                std::string call = register_name(texture) + ".Load(" +
                                   vector_type(Domain::integer, coordinates) +
                                   "(" + location + "))";
                std::string swizzle;
                if (texture.selection == SelectionMode::swizzle) {
                    for (int index = 0; index < 4; ++index) {
                        if ((dest_mask() & (1 << index)) != 0) {
                            swizzle.push_back(
                                kComponents[texture.swizzle[index]]);
                        }
                    }
                }
                if (!swizzle.empty()) {
                    call += "." + swizzle;
                }
                text += format(
                    "    %s = %s;\n",
                    destination_expression(dest()).c_str(),
                    wrap_for_destination(context, dest(), call, Domain::real)
                        .c_str());
                break;
            }
            case opcode::kResInfo: {
                if (ops.size() < 3) {
                    break;
                }
                const Operand& texture = ops[2];
                text += format(
                    "    { uint rw, rh, rl; %s.GetDimensions(0, rw, rh, rl); "
                    "%s = %s; }\n",
                    register_name(texture).c_str(),
                    destination_expression(dest()).c_str(),
                    wrap_for_destination(
                        context,
                        dest(),
                        "float4(rw, rh, 0, rl)." +
                            (dest_mask() == 0xF
                                 ? std::string("xyzw")
                                 : mask_letters(dest_mask())),
                        Domain::real)
                        .c_str());
                break;
            }
            default:
                result->unsupported_opcode = code;
                result->failure =
                    format("opcode %u sem traducao", static_cast<unsigned>(code));
                return false;
        }
    }

    if (!options.injection.empty()) {
        text += "\n";
        text += options.injection;
        text += "\n";
    }
    text += "}\n";

    result->source = std::move(text);
    return true;
}

}
}
