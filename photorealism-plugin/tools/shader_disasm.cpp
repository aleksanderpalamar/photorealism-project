// Prova em bytecode para refatoracoes de shader.
//
// Este utilitario compila um shader exatamente como o plugin compila em tempo
// de execucao: mesmo d3dcompiler_47.dll resolvido por LoadLibraryW, mesmo
// D3D_COMPILE_STANDARD_FILE_INCLUDE e mesmos flags de postprocess.cpp. Em
// seguida imprime o disassembly em stdout.
//
// Qualquer divergencia de compilador, include path ou flag invalidaria a
// comparacao, entao os tres pontos ficam presos ao que o plugin usa.
//
// Uso: shader_disasm.exe <arquivo.hlsl> <entry_point> <perfil>

#include <d3dcompiler.h>
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace {

using CompileFromFileFunction = decltype(&D3DCompileFromFile);
using DisassembleFunction = decltype(&D3DDisassemble);

// O mesmo par de flags usado em postprocess.cpp:1076.
constexpr UINT kCompileFlags =
    D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

constexpr int kMaximumPathLength = 32768;

void print_blob(ID3DBlob* blob) {
    if (blob == nullptr) {
        return;
    }
    std::fwrite(
        blob->GetBufferPointer(), 1, blob->GetBufferSize(), stderr);
    std::fputc('\n', stderr);
}

bool widen_path(const char* narrow, wchar_t* wide, int capacity) {
    const int written = MultiByteToWideChar(
        CP_UTF8, 0, narrow, -1, wide, capacity);
    return written > 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(
            stderr,
            "Uso: shader_disasm.exe <arquivo.hlsl> <entry_point> <perfil>\n");
        return 2;
    }

    const HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
    if (compiler == nullptr) {
        std::fprintf(
            stderr,
            "Nao foi possivel carregar d3dcompiler_47.dll: %lu.\n",
            GetLastError());
        return 3;
    }

    const auto compile_from_file = reinterpret_cast<CompileFromFileFunction>(
        reinterpret_cast<void*>(
            GetProcAddress(compiler, "D3DCompileFromFile")));
    const auto disassemble = reinterpret_cast<DisassembleFunction>(
        reinterpret_cast<void*>(GetProcAddress(compiler, "D3DDisassemble")));
    if (compile_from_file == nullptr || disassemble == nullptr) {
        std::fprintf(
            stderr,
            "d3dcompiler_47.dll nao exporta D3DCompileFromFile/"
            "D3DDisassemble.\n");
        return 3;
    }

    static wchar_t wide_path[kMaximumPathLength];
    if (!widen_path(argv[1], wide_path, kMaximumPathLength)) {
        std::fprintf(stderr, "Caminho invalido: %s.\n", argv[1]);
        return 2;
    }

    ID3DBlob* bytecode = nullptr;
    ID3DBlob* errors = nullptr;
    HRESULT result = compile_from_file(
        wide_path,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        argv[2],
        argv[3],
        kCompileFlags,
        0,
        &bytecode,
        &errors);
    if (FAILED(result) || bytecode == nullptr) {
        std::fprintf(
            stderr,
            "Falha ao compilar %s:%s (%s): 0x%08X.\n",
            argv[1],
            argv[2],
            argv[3],
            static_cast<unsigned>(result));
        print_blob(errors);
        if (errors != nullptr) {
            errors->Release();
        }
        if (bytecode != nullptr) {
            bytecode->Release();
        }
        return 1;
    }
    if (errors != nullptr) {
        errors->Release();
        errors = nullptr;
    }

    ID3DBlob* text = nullptr;
    result = disassemble(
        bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(),
        0,
        nullptr,
        &text);
    if (FAILED(result) || text == nullptr) {
        std::fprintf(
            stderr,
            "Falha ao desmontar %s:%s: 0x%08X.\n",
            argv[1],
            argv[2],
            static_cast<unsigned>(result));
        bytecode->Release();
        if (text != nullptr) {
            text->Release();
        }
        return 1;
    }

    std::printf("=== %s : %s : %s ===\n", argv[1], argv[2], argv[3]);
    std::fwrite(text->GetBufferPointer(), 1, text->GetBufferSize(), stdout);
    std::fputc('\n', stdout);

    text->Release();
    bytecode->Release();
    return 0;
}
