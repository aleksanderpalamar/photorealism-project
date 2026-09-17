#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace photorealism {
namespace shader_patch {

enum class ComponentType : std::uint32_t {
    unknown = 0,
    uint32 = 1,
    sint32 = 2,
    float32 = 3,
};

struct SignatureElement {
    std::string semantic_name;
    std::uint32_t semantic_index = 0;
    std::uint32_t system_value = 0;
    ComponentType component_type = ComponentType::unknown;
    std::uint32_t register_index = 0;
    std::uint8_t mask = 0;
    std::uint8_t read_write_mask = 0;
};

struct Chunk {
    char tag[5] = {};
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
};

class Container {
  public:
    bool parse(const void* bytecode, std::size_t size);

    const Chunk* find(const char* tag) const;

    const std::vector<SignatureElement>& inputs() const { return inputs_; }
    const std::vector<SignatureElement>& outputs() const { return outputs_; }
    const Chunk* code() const { return code_; }

    std::uint64_t hash() const { return hash_; }
    std::size_t size() const { return size_; }
    const std::uint8_t* bytes() const { return bytes_; }

  private:
    const std::uint8_t* bytes_ = nullptr;
    std::size_t size_ = 0;
    std::uint64_t hash_ = 0;
    std::vector<Chunk> chunks_;
    std::vector<SignatureElement> inputs_;
    std::vector<SignatureElement> outputs_;
    const Chunk* code_ = nullptr;
};

std::uint64_t hash_bytes(const void* data, std::size_t size);

const char* component_type_name(ComponentType type);

std::string mask_to_swizzle(std::uint8_t mask);

}
}
