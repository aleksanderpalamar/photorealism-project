#include "dxbc.hpp"

#include <cstring>

namespace photorealism {
namespace shader_patch {
namespace {

constexpr std::uint64_t kHashOffset = 1469598103934665603ULL;
constexpr std::uint64_t kHashPrime = 1099511628211ULL;

std::uint32_t read_u32(const std::uint8_t* base, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

bool read_signature(
    const Chunk& chunk, std::vector<SignatureElement>* elements) {
    if (chunk.size < 8) {
        return false;
    }
    const std::uint32_t count = read_u32(chunk.data, 0);
    const std::uint32_t first = read_u32(chunk.data, 4);
    if (count > 64) {
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::size_t base = first + index * 24;
        if (base + 24 > chunk.size) {
            return false;
        }
        SignatureElement element;
        const std::uint32_t name_offset = read_u32(chunk.data, base);
        if (name_offset >= chunk.size) {
            return false;
        }
        const char* name =
            reinterpret_cast<const char*>(chunk.data) + name_offset;
        const std::size_t available = chunk.size - name_offset;
        const std::size_t length = ::strnlen(name, available);
        element.semantic_name.assign(name, length);
        element.semantic_index = read_u32(chunk.data, base + 4);
        element.system_value = read_u32(chunk.data, base + 8);
        element.component_type =
            static_cast<ComponentType>(read_u32(chunk.data, base + 12));
        element.register_index = read_u32(chunk.data, base + 16);
        element.mask = chunk.data[base + 20];
        element.read_write_mask = chunk.data[base + 21];
        elements->push_back(element);
    }
    return true;
}

}  // namespace

std::uint64_t hash_bytes(const void* data, std::size_t size) {
    const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data);
    std::uint64_t value = kHashOffset;
    for (std::size_t index = 0; index < size; ++index) {
        value ^= bytes[index];
        value *= kHashPrime;
    }
    return value;
}

const char* component_type_name(ComponentType type) {
    switch (type) {
        case ComponentType::uint32:
            return "uint";
        case ComponentType::sint32:
            return "int";
        case ComponentType::float32:
            return "float";
        default:
            return "float";
    }
}

std::string mask_to_swizzle(std::uint8_t mask) {
    static const char kComponents[] = "xyzw";
    std::string result;
    for (int index = 0; index < 4; ++index) {
        if ((mask & (1 << index)) != 0) {
            result.push_back(kComponents[index]);
        }
    }
    return result;
}

bool Container::parse(const void* bytecode, std::size_t size) {
    bytes_ = static_cast<const std::uint8_t*>(bytecode);
    size_ = size;
    chunks_.clear();
    inputs_.clear();
    outputs_.clear();
    code_ = nullptr;
    hash_ = 0;

    if (bytes_ == nullptr || size_ < 32) {
        return false;
    }
    if (std::memcmp(bytes_, "DXBC", 4) != 0) {
        return false;
    }
    const std::uint32_t total = read_u32(bytes_, 24);
    if (total > size_) {
        return false;
    }
    const std::uint32_t count = read_u32(bytes_, 28);
    if (count == 0 || count > 32 || 32 + count * 4 > size_) {
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint32_t offset = read_u32(bytes_, 32 + index * 4);
        if (offset + 8 > size_) {
            return false;
        }
        Chunk chunk;
        std::memcpy(chunk.tag, bytes_ + offset, 4);
        chunk.size = read_u32(bytes_, offset + 4);
        chunk.data = bytes_ + offset + 8;
        if (offset + 8 + chunk.size > size_) {
            return false;
        }
        chunks_.push_back(chunk);
    }

    hash_ = hash_bytes(bytes_, total);

    const Chunk* isgn = find("ISGN");
    if (isgn == nullptr) {
        isgn = find("ISG1");
    }
    if (isgn != nullptr && !read_signature(*isgn, &inputs_)) {
        return false;
    }
    const Chunk* osgn = find("OSGN");
    if (osgn == nullptr) {
        osgn = find("OSG5");
    }
    if (osgn == nullptr) {
        osgn = find("OSG1");
    }
    if (osgn != nullptr && !read_signature(*osgn, &outputs_)) {
        return false;
    }
    code_ = find("SHEX");
    if (code_ == nullptr) {
        code_ = find("SHDR");
    }
    return code_ != nullptr;
}

const Chunk* Container::find(const char* tag) const {
    for (const Chunk& chunk : chunks_) {
        if (std::memcmp(chunk.tag, tag, 4) == 0) {
            return &chunk;
        }
    }
    return nullptr;
}

}
}
