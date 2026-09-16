// Compila em lote os HLSL gerados pelo transpilador, num unico processo, com o
// mesmo d3dcompiler_47.dll que o plugin resolve em tempo de execucao.
//
// Uso: shader_batch_compile.exe <lista.txt>
// A lista tem um caminho de .hlsl por linha. Sai com 1 se algum falhar.

#include <d3dcompiler.h>
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using CompileFunction = decltype(&D3DCompile);

constexpr UINT kCompileFlags =
    D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

std::string read_file(const char* path) {
    std::string data;
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return data;
    }
    char buffer[8192];
    std::size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        data.append(buffer, read);
    }
    std::fclose(file);
    return data;
}

std::string first_line(const char* text) {
    if (text == nullptr) {
        return std::string();
    }
    const char* end = std::strchr(text, '\n');
    return end == nullptr ? std::string(text) : std::string(text, end - text);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "uso: shader_batch_compile <lista.txt>\n");
        return 2;
    }

    HMODULE library = LoadLibraryW(L"d3dcompiler_47.dll");
    if (library == nullptr) {
        std::fprintf(stderr, "d3dcompiler_47.dll indisponivel\n");
        return 2;
    }
    CompileFunction compile =
        reinterpret_cast<CompileFunction>(GetProcAddress(library, "D3DCompile"));
    if (compile == nullptr) {
        std::fprintf(stderr, "D3DCompile indisponivel\n");
        return 2;
    }

    const std::string list = read_file(argv[1]);
    std::vector<std::string> paths;
    std::size_t start = 0;
    while (start <= list.size()) {
        const std::size_t end = list.find('\n', start);
        std::string line = list.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (!line.empty()) {
            paths.push_back(line);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    unsigned passed = 0;
    unsigned failed = 0;
    for (const std::string& path : paths) {
        const std::string source = read_file(path.c_str());
        if (source.empty()) {
            std::printf("FALHA %s: arquivo vazio\n", path.c_str());
            ++failed;
            continue;
        }
        ID3DBlob* code = nullptr;
        ID3DBlob* errors = nullptr;
        const HRESULT result = compile(
            source.data(),
            source.size(),
            path.c_str(),
            nullptr,
            nullptr,
            "main",
            "ps_5_0",
            kCompileFlags,
            0,
            &code,
            &errors);
        if (FAILED(result)) {
            const char* text =
                errors != nullptr
                    ? static_cast<const char*>(errors->GetBufferPointer())
                    : nullptr;
            std::printf(
                "FALHA %s: %s\n", path.c_str(), first_line(text).c_str());
            ++failed;
        } else {
            ++passed;
        }
        if (code != nullptr) {
            code->Release();
        }
        if (errors != nullptr) {
            errors->Release();
        }
    }

    std::printf("compilados=%u falhas=%u\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
