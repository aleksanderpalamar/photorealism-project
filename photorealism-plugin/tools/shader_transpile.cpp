#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/shader_patch/dxbc.hpp"
#include "../src/shader_patch/gbuffer_patch.hpp"
#include "../src/shader_patch/hlsl_emit.hpp"
#include "../src/shader_patch/shex.hpp"

namespace {

std::vector<unsigned char> read_file(const char* path) {
    std::vector<unsigned char> data;
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return data;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size > 0) {
        data.resize(static_cast<std::size_t>(size));
        if (std::fread(data.data(), 1, data.size(), file) != data.size()) {
            data.clear();
        }
    }
    std::fclose(file);
    return data;
}

const unsigned char* find_container(
    const std::vector<unsigned char>& data, std::size_t* size) {
    for (std::size_t index = 0; index + 4 <= data.size(); ++index) {
        if (std::memcmp(data.data() + index, "DXBC", 4) == 0) {
            *size = data.size() - index;
            return data.data() + index;
        }
    }
    return nullptr;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(
            stderr,
            "uso: shader_transpile <arquivo.fso|.dxbc> [--inject <lib.hlsl>] "
            "[--report]\n");
        return 2;
    }

    const char* library_path = nullptr;
    bool report_only = false;
    for (int index = 2; index < argc; ++index) {
        if (std::strcmp(argv[index], "--inject") == 0 && index + 1 < argc) {
            library_path = argv[++index];
        } else if (std::strcmp(argv[index], "--report") == 0) {
            report_only = true;
        }
    }

    const std::vector<unsigned char> file = read_file(argv[1]);
    if (file.empty()) {
        std::fprintf(stderr, "nao foi possivel ler %s\n", argv[1]);
        return 2;
    }

    std::size_t size = 0;
    const unsigned char* bytes = find_container(file, &size);
    if (bytes == nullptr) {
        std::fprintf(stderr, "sem contentor DXBC em %s\n", argv[1]);
        return 2;
    }

    photorealism::shader_patch::Container container;
    if (!container.parse(bytes, size)) {
        std::fprintf(stderr, "DXBC invalido em %s\n", argv[1]);
        return 1;
    }

    photorealism::shader_patch::Program program;
    if (!photorealism::shader_patch::decode_program(
            container.code()->data, container.code()->size, &program)) {
        std::fprintf(stderr, "SHEX nao decodificou em %s\n", argv[1]);
        return 1;
    }

    const photorealism::shader_patch::GBufferInfo info =
        photorealism::shader_patch::inspect_gbuffer_shader(container, program);

    if (report_only) {
        std::printf(
            "%s\telegivel=%s\t%s\talbedo=%s/%s/%s\tsegundo=%s\tescala=%.0f\n",
            argv[1],
            info.eligible ? "sim" : "nao",
            info.eligible ? "" : info.reason.c_str(),
            info.albedo_texture.c_str(),
            info.albedo_sampler.c_str(),
            info.albedo_uv.c_str(),
            info.has_second_albedo ? info.second_texture.c_str() : "-",
            info.reflection_scale);
        return info.eligible ? 0 : 1;
    }

    photorealism::shader_patch::EmitOptions options;
    if (library_path != nullptr) {
        const std::vector<unsigned char> library = read_file(library_path);
        if (library.empty()) {
            std::fprintf(stderr, "biblioteca vazia: %s\n", library_path);
            return 2;
        }
        options.prologue.assign(library.begin(), library.end());
        options.injection = photorealism::shader_patch::build_injection_call(info);
        if (options.injection.empty()) {
            std::fprintf(
                stderr, "%s nao e alvo: %s\n", argv[1], info.reason.c_str());
            return 1;
        }
    }

    photorealism::shader_patch::EmitResult result;
    if (!photorealism::shader_patch::emit_hlsl(
            container, program, options, &result)) {
        std::fprintf(
            stderr,
            "%s: %s\n",
            argv[1],
            result.failure.c_str());
        return 1;
    }

    std::fwrite(result.source.data(), 1, result.source.size(), stdout);
    return 0;
}
