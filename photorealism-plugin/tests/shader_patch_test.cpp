#include "../src/shader_patch/dxbc.cpp"
#include "../src/shader_patch/gbuffer_patch.cpp"
#include "../src/shader_patch/hlsl_emit.cpp"
#include "../src/shader_patch/shex.cpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace photorealism::shader_patch;

namespace {

struct SignatureSpec {
    const char* name;
    std::uint32_t index;
    std::uint32_t component_type;
    std::uint32_t reg;
    std::uint8_t mask;
};

void push_u32(std::vector<std::uint8_t>* out, std::uint32_t value) {
    out->push_back(static_cast<std::uint8_t>(value & 0xFF));
    out->push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out->push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out->push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

std::vector<std::uint8_t> build_signature(
    const std::vector<SignatureSpec>& elements) {
    std::vector<std::uint8_t> body;
    push_u32(&body, static_cast<std::uint32_t>(elements.size()));
    push_u32(&body, 8);

    const std::size_t names_start = 8 + elements.size() * 24;
    std::vector<std::uint8_t> names;
    std::vector<std::uint32_t> offsets;
    for (const SignatureSpec& element : elements) {
        offsets.push_back(
            static_cast<std::uint32_t>(names_start + names.size()));
        const std::size_t length = std::strlen(element.name);
        names.insert(names.end(), element.name, element.name + length);
        names.push_back(0);
    }

    for (std::size_t index = 0; index < elements.size(); ++index) {
        const SignatureSpec& element = elements[index];
        push_u32(&body, offsets[index]);
        push_u32(&body, element.index);
        push_u32(&body, 0);
        push_u32(&body, element.component_type);
        push_u32(&body, element.reg);
        body.push_back(element.mask);
        body.push_back(element.mask);
        body.push_back(0);
        body.push_back(0);
    }
    body.insert(body.end(), names.begin(), names.end());
    return body;
}

std::uint32_t operand_token(
    std::uint32_t type,
    std::uint32_t dimension,
    std::uint32_t selection,
    std::uint32_t selection_bits) {
    std::uint32_t token = 2u;
    token |= (selection & 3u) << 2;
    token |= (selection_bits & 0xFFu) << 4;
    token |= (type & 0xFFu) << 12;
    token |= (dimension & 3u) << 20;
    return token;
}

std::uint32_t swizzle_bits(const char* pattern) {
    std::uint32_t bits = 0;
    for (int index = 0; index < 4; ++index) {
        const char component = pattern[index];
        const std::uint32_t value = component == 'x'   ? 0u
                                    : component == 'y' ? 1u
                                    : component == 'z' ? 2u
                                                       : 3u;
        bits |= value << (2 * index);
    }
    return bits;
}

class CodeBuilder {
  public:
    CodeBuilder() {
        tokens_.push_back(0x00000040u);
        tokens_.push_back(0);
    }

    void declare_sampler(std::uint32_t slot) {
        emit(opcode::kDclSampler, 0, {operand_token(6, 1, 0, 0), slot});
    }

    void declare_resource(std::uint32_t slot) {
        emit(
            opcode::kDclResource,
            3u,
            {operand_token(7, 1, 1, swizzle_bits("xyzw")), slot, 0x00005555u});
    }

    void declare_constant_buffer(std::uint32_t slot, std::uint32_t size) {
        emit(
            opcode::kDclConstantBuffer,
            0,
            {operand_token(8, 2, 1, swizzle_bits("xyzw")), slot, size});
    }

    void declare_input(std::uint32_t reg, std::uint8_t mask) {
        emit(opcode::kDclInputPs, 1u, {operand_token(1, 1, 0, mask), reg});
    }

    void declare_output(std::uint32_t reg) {
        emit(opcode::kDclOutput, 0, {operand_token(2, 1, 0, 0xF), reg});
    }

    void declare_temps(std::uint32_t count) {
        emit(opcode::kDclTemps, 0, {count});
    }

    void sample(
        std::uint32_t destination,
        std::uint32_t coordinate,
        std::uint32_t resource,
        std::uint32_t sampler) {
        emit(
            opcode::kSample,
            0,
            {operand_token(0, 1, 0, 0xF),
             destination,
             operand_token(1, 1, 1, swizzle_bits("xyxx")),
             coordinate,
             operand_token(7, 1, 1, swizzle_bits("xyzw")),
             resource,
             operand_token(6, 1, 0, 0),
             sampler});
    }

    void move_input_to_output(
        std::uint32_t output, std::uint8_t mask, std::uint32_t input) {
        emit(
            opcode::kMov,
            0,
            {operand_token(2, 1, 0, mask),
             output,
             operand_token(1, 1, 1, swizzle_bits("xyzw")),
             input});
    }

    void move_literal_to_output(
        std::uint32_t output,
        std::uint8_t mask,
        std::uint32_t x,
        std::uint32_t y) {
        emit(
            opcode::kMov,
            0,
            {operand_token(2, 1, 0, mask),
             output,
             operand_token(4, 0, 1, swizzle_bits("xyzw")),
             x,
             y,
             0,
             0});
    }

    void ret() { emit(opcode::kRet, 0, {}); }

    std::vector<std::uint8_t> finish() {
        tokens_[1] = static_cast<std::uint32_t>(tokens_.size());
        std::vector<std::uint8_t> bytes;
        for (std::uint32_t token : tokens_) {
            push_u32(&bytes, token);
        }
        return bytes;
    }

  private:
    void emit(
        std::uint32_t code,
        std::uint32_t control,
        std::initializer_list<std::uint32_t> body) {
        const std::uint32_t length =
            static_cast<std::uint32_t>(body.size()) + 1u;
        tokens_.push_back(
            (code & 0x7FFu) | ((control & 0x1FFFu) << 11) | (length << 24));
        for (std::uint32_t value : body) {
            tokens_.push_back(value);
        }
    }

    std::vector<std::uint32_t> tokens_;
};

std::vector<std::uint8_t> build_container(
    const std::vector<SignatureSpec>& inputs,
    const std::vector<SignatureSpec>& outputs,
    const std::vector<std::uint8_t>& code) {
    const std::vector<std::uint8_t> isgn = build_signature(inputs);
    const std::vector<std::uint8_t> osgn = build_signature(outputs);

    std::vector<std::uint8_t> blob;
    blob.insert(blob.end(), {'D', 'X', 'B', 'C'});
    for (int index = 0; index < 16; ++index) {
        blob.push_back(0);
    }
    push_u32(&blob, 1);
    const std::size_t total_offset = blob.size();
    push_u32(&blob, 0);
    push_u32(&blob, 3);

    const std::size_t table_offset = blob.size();
    push_u32(&blob, 0);
    push_u32(&blob, 0);
    push_u32(&blob, 0);

    std::uint32_t offsets[3] = {};

    offsets[0] = static_cast<std::uint32_t>(blob.size());
    blob.insert(blob.end(), {'I', 'S', 'G', 'N'});
    push_u32(&blob, static_cast<std::uint32_t>(isgn.size()));
    blob.insert(blob.end(), isgn.begin(), isgn.end());

    offsets[1] = static_cast<std::uint32_t>(blob.size());
    blob.insert(blob.end(), {'O', 'S', 'G', 'N'});
    push_u32(&blob, static_cast<std::uint32_t>(osgn.size()));
    blob.insert(blob.end(), osgn.begin(), osgn.end());

    offsets[2] = static_cast<std::uint32_t>(blob.size());
    blob.insert(blob.end(), {'S', 'H', 'E', 'X'});
    push_u32(&blob, static_cast<std::uint32_t>(code.size()));
    blob.insert(blob.end(), code.begin(), code.end());

    for (int index = 0; index < 3; ++index) {
        const std::size_t position = table_offset + index * 4;
        blob[position] = static_cast<std::uint8_t>(offsets[index] & 0xFF);
        blob[position + 1] =
            static_cast<std::uint8_t>((offsets[index] >> 8) & 0xFF);
        blob[position + 2] =
            static_cast<std::uint8_t>((offsets[index] >> 16) & 0xFF);
        blob[position + 3] =
            static_cast<std::uint8_t>((offsets[index] >> 24) & 0xFF);
    }
    const std::uint32_t total = static_cast<std::uint32_t>(blob.size());
    blob[total_offset] = static_cast<std::uint8_t>(total & 0xFF);
    blob[total_offset + 1] = static_cast<std::uint8_t>((total >> 8) & 0xFF);
    blob[total_offset + 2] = static_cast<std::uint8_t>((total >> 16) & 0xFF);
    blob[total_offset + 3] = static_cast<std::uint8_t>((total >> 24) & 0xFF);
    return blob;
}

std::vector<SignatureSpec> gbuffer_inputs() {
    return {
        {"COLOR", 0, 3, 0, 0xF},
        {"SV_Position", 0, 3, 1, 0xF},
        {"TEXCOORD", 0, 3, 2, 0x7},
        {"TEXCOORD", 1, 3, 3, 0x7},
        {"TEXCOORD", 2, 3, 4, 0x3},
    };
}

std::vector<SignatureSpec> gbuffer_outputs() {
    return {
        {"SV_Target", 0, 3, 0, 0xF},
        {"SV_Target", 1, 3, 1, 0xF},
        {"SV_Target", 2, 3, 2, 0xF},
        {"SV_Target", 3, 1, 3, 0xF},
    };
}

std::vector<std::uint8_t> gbuffer_code() {
    CodeBuilder builder;
    builder.declare_constant_buffer(0, 4);
    builder.declare_sampler(0);
    builder.declare_resource(6);
    builder.declare_input(0, 0x7);
    builder.declare_input(2, 0x7);
    builder.declare_input(3, 0x4);
    builder.declare_input(4, 0x3);
    builder.declare_output(0);
    builder.declare_output(1);
    builder.declare_output(2);
    builder.declare_output(3);
    builder.declare_temps(2);
    builder.move_input_to_output(0, 0x7, 2);
    builder.sample(0, 4, 6, 0);
    builder.move_literal_to_output(3, 0x3, 0, 32);
    builder.ret();
    return builder.finish();
}

bool contains(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

void test_container_parses_signatures() {
    const std::vector<std::uint8_t> blob =
        build_container(gbuffer_inputs(), gbuffer_outputs(), gbuffer_code());
    Container container;
    assert(container.parse(blob.data(), blob.size()));
    assert(container.inputs().size() == 5);
    assert(container.outputs().size() == 4);
    assert(container.inputs()[1].semantic_name == "SV_Position");
    assert(container.outputs()[3].component_type == ComponentType::uint32);
    assert(container.code() != nullptr);
    assert(container.hash() != 0);
}

void test_program_decodes() {
    const std::vector<std::uint8_t> blob =
        build_container(gbuffer_inputs(), gbuffer_outputs(), gbuffer_code());
    Container container;
    assert(container.parse(blob.data(), blob.size()));
    Program program;
    assert(decode_program(
        container.code()->data, container.code()->size, &program));

    unsigned samples = 0;
    for (const Instruction& instruction : program.instructions) {
        if (instruction.opcode == opcode::kSample) {
            ++samples;
        }
    }
    assert(samples == 1);
    assert(program.temp_count == 2);
    assert(program.declared_samplers.size() == 1);
    assert(program.declared_samplers[0] == 0);
    assert(program.declared_resources.size() == 1);
    assert(program.declared_resources[0] == 6);
}

void test_gbuffer_is_recognised() {
    const std::vector<std::uint8_t> blob =
        build_container(gbuffer_inputs(), gbuffer_outputs(), gbuffer_code());
    Container container;
    assert(container.parse(blob.data(), blob.size()));
    Program program;
    assert(decode_program(
        container.code()->data, container.code()->size, &program));

    const GBufferInfo info = inspect_gbuffer_shader(container, program);
    assert(info.eligible);
    assert(info.has_albedo);
    assert(info.albedo_texture == "t6");
    assert(info.albedo_sampler == "s0");
    assert(info.albedo_uv == "v4.xy");
    assert(info.position_input == "v1");
    assert(info.blend_input == "v0");

    const std::string call = build_injection_call(info);
    assert(contains(call, "photorealism_wet_surface(o0, o1, o2, o3, v1, t6, s0, v4.xy"));
}

void test_single_target_shader_is_rejected() {
    const std::vector<SignatureSpec> outputs = {{"SV_Target", 0, 3, 0, 0xF}};
    CodeBuilder builder;
    builder.declare_sampler(0);
    builder.declare_resource(6);
    builder.declare_input(4, 0x3);
    builder.declare_output(0);
    builder.declare_temps(1);
    builder.sample(0, 4, 6, 0);
    builder.ret();

    const std::vector<std::uint8_t> blob =
        build_container(gbuffer_inputs(), outputs, builder.finish());
    Container container;
    assert(container.parse(blob.data(), blob.size()));
    Program program;
    assert(decode_program(
        container.code()->data, container.code()->size, &program));

    const GBufferInfo info = inspect_gbuffer_shader(container, program);
    assert(!info.eligible);
    assert(!info.reason.empty());
    assert(build_injection_call(info).empty());
}

void test_emitted_hlsl_shape() {
    const std::vector<std::uint8_t> blob =
        build_container(gbuffer_inputs(), gbuffer_outputs(), gbuffer_code());
    Container container;
    assert(container.parse(blob.data(), blob.size()));
    Program program;
    assert(decode_program(
        container.code()->data, container.code()->size, &program));

    const GBufferInfo info = inspect_gbuffer_shader(container, program);
    EmitOptions options;
    options.prologue = "// biblioteca";
    options.injection = build_injection_call(info);
    EmitResult result;
    assert(emit_hlsl(container, program, options, &result));
    assert(result.failure.empty());
    assert(!result.early_return);

    assert(contains(result.source, "SamplerState s0 : register(s0);"));
    assert(contains(result.source, "Texture2D<float4> t6 : register(t6);"));
    assert(contains(result.source, "register(b0) { float4 cb0[4]; }"));
    assert(contains(result.source, "float4 v1 : SV_Position"));
    assert(contains(result.source, "float3 v2 : TEXCOORD0"));
    assert(contains(result.source, "out float4 o0 : SV_Target0"));
    assert(contains(result.source, "out uint4 o3 : SV_Target3"));
    assert(contains(result.source, "r0.xyzw = t6.Sample(s0, v4.xy).xyzw;"));
    assert(contains(result.source, "o3.xy = uint2(0u, 32u);"));
    assert(contains(result.source, "photorealism_wet_surface("));
    assert(contains(result.source, "// biblioteca"));

    const std::size_t injection = result.source.find("photorealism_wet_surface(o0");
    const std::size_t sample = result.source.find("t6.Sample(s0");
    assert(sample < injection);
}

void test_packed_input_registers_are_assembled() {
    std::vector<SignatureSpec> inputs = gbuffer_inputs();
    inputs.push_back({"TEXCOORD", 7, 3, 4, 0x4});

    const std::vector<std::uint8_t> blob =
        build_container(inputs, gbuffer_outputs(), gbuffer_code());
    Container container;
    assert(container.parse(blob.data(), blob.size()));
    Program program;
    assert(decode_program(
        container.code()->data, container.code()->size, &program));

    EmitOptions options;
    EmitResult result;
    assert(emit_hlsl(container, program, options, &result));
    assert(contains(result.source, "float2 v4_0 : TEXCOORD2"));
    assert(contains(result.source, "float1 v4_1 : TEXCOORD7"));
    assert(contains(result.source, "float4 v4 = 0;"));
    assert(contains(result.source, "v4.xy = v4_0;"));
    assert(contains(result.source, "v4.z = v4_1;"));
}

void test_hash_follows_the_bytes() {
    const std::vector<std::uint8_t> blob =
        build_container(gbuffer_inputs(), gbuffer_outputs(), gbuffer_code());
    Container first;
    Container second;
    assert(first.parse(blob.data(), blob.size()));
    assert(second.parse(blob.data(), blob.size()));
    assert(first.hash() == second.hash());

    std::vector<std::uint8_t> changed = blob;
    changed[changed.size() - 5] ^= 0xFFu;
    Container third;
    if (third.parse(changed.data(), changed.size())) {
        assert(third.hash() != first.hash());
    }
}

}  // namespace

int main() {
    test_container_parses_signatures();
    test_program_decodes();
    test_gbuffer_is_recognised();
    test_single_target_shader_is_rejected();
    test_emitted_hlsl_shape();
    test_packed_input_registers_are_assembled();
    test_hash_follows_the_bytes();
    std::printf("shader_patch_test: ok\n");
    return 0;
}
