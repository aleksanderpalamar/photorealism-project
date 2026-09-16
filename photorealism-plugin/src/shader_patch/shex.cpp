#include "shex.hpp"

#include <cstring>

namespace photorealism {
namespace shader_patch {
namespace {

constexpr std::uint32_t kExtendedBit = 0x80000000u;
constexpr std::uint32_t kSaturateBit = 1u << 13;

class TokenReader {
  public:
    TokenReader(const std::uint32_t* tokens, std::size_t count)
        : tokens_(tokens), count_(count) {}

    bool at_end() const { return cursor_ >= count_; }
    std::size_t cursor() const { return cursor_; }
    void seek(std::size_t position) { cursor_ = position; }

    bool peek(std::uint32_t* value) const {
        if (cursor_ >= count_) {
            return false;
        }
        *value = tokens_[cursor_];
        return true;
    }

    bool next(std::uint32_t* value) {
        if (cursor_ >= count_) {
            return false;
        }
        *value = tokens_[cursor_++];
        return true;
    }

  private:
    const std::uint32_t* tokens_;
    std::size_t count_;
    std::size_t cursor_ = 0;
};

bool read_operand(TokenReader* reader, Operand* operand) {
    std::uint32_t token = 0;
    if (!reader->next(&token)) {
        return false;
    }
    *operand = Operand();
    const std::uint32_t num_components = token & 3u;
    operand->component_count = num_components == 2 ? 4 : num_components;
    operand->type = static_cast<OperandType>((token >> 12) & 0xFFu);
    const std::uint32_t dimension = (token >> 20) & 3u;

    if (num_components == 2) {
        const std::uint32_t selection = (token >> 2) & 3u;
        if (selection == 0) {
            operand->selection = SelectionMode::mask;
            operand->mask = static_cast<std::uint8_t>((token >> 4) & 0xFu);
        } else if (selection == 1) {
            operand->selection = SelectionMode::swizzle;
            const std::uint32_t swizzle = (token >> 4) & 0xFFu;
            for (int index = 0; index < 4; ++index) {
                operand->swizzle[index] =
                    static_cast<std::uint8_t>((swizzle >> (2 * index)) & 3u);
            }
        } else if (selection == 2) {
            operand->selection = SelectionMode::select_one;
            operand->select_one = static_cast<std::uint8_t>((token >> 4) & 3u);
        }
    }

    if ((token & kExtendedBit) != 0) {
        std::uint32_t extended = 0;
        if (!reader->next(&extended)) {
            return false;
        }
        if ((extended & 0x3Fu) == 1) {
            operand->modifier =
                static_cast<Modifier>((extended >> 6) & 0xFFu);
        }
        while ((extended & kExtendedBit) != 0) {
            if (!reader->next(&extended)) {
                return false;
            }
        }
    }

    if (operand->type == OperandType::immediate32) {
        const std::uint32_t count = num_components == 2 ? 4u : 1u;
        for (std::uint32_t index = 0; index < count; ++index) {
            std::uint32_t raw = 0;
            if (!reader->next(&raw)) {
                return false;
            }
            operand->immediate_raw[index] = raw;
            std::memcpy(&operand->immediate_float[index], &raw, sizeof(raw));
        }
        return true;
    }
    if (operand->type == OperandType::immediate64) {
        const std::uint32_t count = num_components == 2 ? 8u : 2u;
        for (std::uint32_t index = 0; index < count; ++index) {
            std::uint32_t raw = 0;
            if (!reader->next(&raw)) {
                return false;
            }
        }
        return true;
    }

    for (std::uint32_t dim = 0; dim < dimension; ++dim) {
        const std::uint32_t representation = (token >> (22 + 3 * dim)) & 7u;
        Index index;
        if (representation == 0) {
            if (!reader->next(&index.immediate)) {
                return false;
            }
        } else if (representation == 1) {
            std::uint32_t low = 0;
            std::uint32_t high = 0;
            if (!reader->next(&low) || !reader->next(&high)) {
                return false;
            }
            index.immediate = low;
        } else if (representation == 2) {
            Operand relative;
            if (!read_operand(reader, &relative)) {
                return false;
            }
            index.relative = true;
            index.relative_register =
                relative.indices.empty() ? 0 : relative.indices[0].immediate;
            index.relative_component =
                relative.selection == SelectionMode::select_one
                    ? relative.select_one
                    : relative.swizzle[0];
        } else if (representation == 3) {
            if (!reader->next(&index.immediate)) {
                return false;
            }
            Operand relative;
            if (!read_operand(reader, &relative)) {
                return false;
            }
            index.relative = true;
            index.relative_register =
                relative.indices.empty() ? 0 : relative.indices[0].immediate;
            index.relative_component =
                relative.selection == SelectionMode::select_one
                    ? relative.select_one
                    : relative.swizzle[0];
        } else {
            return false;
        }
        operand->indices.push_back(index);
    }
    return true;
}

}  // namespace

bool decode_program(
    const std::uint8_t* code, std::size_t size, Program* out) {
    if (code == nullptr || size < 8 || out == nullptr) {
        return false;
    }
    const std::size_t token_count = size / 4;
    std::vector<std::uint32_t> tokens(token_count);
    std::memcpy(tokens.data(), code, token_count * 4);

    out->instructions.clear();
    out->declared_samplers.clear();
    out->declared_resources.clear();
    out->declared_buffers.clear();
    out->indexable_temps.clear();

    const std::uint32_t version = tokens[0];
    out->minor = version & 0xFu;
    out->major = (version >> 4) & 0xFu;
    out->program_type = (version >> 16) & 0xFFFFu;

    std::size_t cursor = 2;
    while (cursor < token_count) {
        const std::uint32_t token = tokens[cursor];
        const std::uint32_t opcode = token & 0x7FFu;

        if (opcode == opcode::kCustomData) {
            if (cursor + 1 >= token_count) {
                break;
            }
            const std::uint32_t length = tokens[cursor + 1];
            cursor += length < 2 ? 2 : length;
            continue;
        }

        std::uint32_t length = (token >> 24) & 0x7Fu;
        if (length == 0 || cursor + length > token_count) {
            break;
        }
        const std::size_t end = cursor + length;

        Instruction instruction;
        instruction.opcode = opcode;
        instruction.saturate = (token & kSaturateBit) != 0;
        instruction.control = (token >> 11) & 0x1FFFu;

        std::size_t body = cursor + 1;
        if ((token & kExtendedBit) != 0) {
            while (body < end && (tokens[body - 1] & kExtendedBit) != 0) {
                ++body;
            }
        }

        if (opcode == opcode::kDclTemps) {
            if (body < end) {
                out->temp_count = tokens[body];
            }
            out->instructions.push_back(std::move(instruction));
            cursor = end;
            continue;
        }
        if (opcode == opcode::kDclIndexableTemp) {
            if (body + 2 < end) {
                out->indexable_temps.emplace_back(tokens[body], tokens[body + 1]);
            }
            out->instructions.push_back(std::move(instruction));
            cursor = end;
            continue;
        }
        if (opcode == opcode::kDclGlobalFlags || opcode == opcode::kNop) {
            out->instructions.push_back(std::move(instruction));
            cursor = end;
            continue;
        }

        TokenReader reader(tokens.data() + body, end - body);
        while (!reader.at_end()) {
            Operand operand;
            const std::size_t before = reader.cursor();
            if (!read_operand(&reader, &operand)) {
                break;
            }
            if (reader.cursor() == before) {
                break;
            }
            instruction.operands.push_back(operand);
            if (opcode == opcode::kDclResource ||
                opcode == opcode::kDclSampler ||
                opcode == opcode::kDclInputPs ||
                opcode == opcode::kDclOutput ||
                opcode == opcode::kDclConstantBuffer) {
                break;
            }
        }

        if (opcode == opcode::kDclSampler && !instruction.operands.empty() &&
            !instruction.operands[0].indices.empty()) {
            out->declared_samplers.push_back(
                instruction.operands[0].indices[0].immediate);
        }
        if (opcode == opcode::kDclResource && !instruction.operands.empty() &&
            !instruction.operands[0].indices.empty()) {
            out->declared_resources.push_back(
                instruction.operands[0].indices[0].immediate);
        }
        if (opcode == opcode::kDclConstantBuffer &&
            !instruction.operands.empty() &&
            instruction.operands[0].indices.size() >= 2) {
            out->declared_buffers.emplace_back(
                instruction.operands[0].indices[0].immediate,
                instruction.operands[0].indices[1].immediate);
        }

        out->instructions.push_back(std::move(instruction));
        cursor = end;
    }

    return !out->instructions.empty();
}

}
}
