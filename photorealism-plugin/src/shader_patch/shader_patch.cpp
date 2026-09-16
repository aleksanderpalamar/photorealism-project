#include "shader_patch.hpp"

#include <d3dcompiler.h>
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>

#include "../runtime.hpp"
#include "dxbc.hpp"
#include "gbuffer_patch.hpp"
#include "hlsl_emit.hpp"
#include "shex.hpp"

namespace photorealism {
namespace shader_patch {
namespace {

using CompileFunction = decltype(&D3DCompile);

constexpr UINT kCompileFlags =
    D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

std::atomic<bool> g_enabled{true};
std::mutex g_mutex;
PatchStatistics g_statistics;
std::unordered_map<std::uint64_t, std::vector<std::uint8_t>> g_memory_cache;
std::unordered_map<std::uint64_t, bool> g_rejected;
std::string g_library;
std::uint64_t g_library_hash = 0;
bool g_library_loaded = false;
bool g_cache_directory_ready = false;
std::wstring g_cache_directory;

CompileFunction resolve_compiler() {
    static CompileFunction compile = []() -> CompileFunction {
        HMODULE library = LoadLibraryW(L"d3dcompiler_47.dll");
        if (library == nullptr) {
            log_message(
                "Patch de shader: d3dcompiler_47.dll indisponivel (%lu).",
                GetLastError());
            return nullptr;
        }
        return reinterpret_cast<CompileFunction>(
            GetProcAddress(library, "D3DCompile"));
    }();
    return compile;
}

std::string read_text_file(const wchar_t* path) {
    std::string contents;
    HANDLE file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return contents;
    }
    LARGE_INTEGER size = {};
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 &&
        size.QuadPart < 4 * 1024 * 1024) {
        contents.resize(static_cast<std::size_t>(size.QuadPart));
        DWORD read = 0;
        if (!ReadFile(
                file,
                contents.data(),
                static_cast<DWORD>(contents.size()),
                &read,
                nullptr) ||
            read != contents.size()) {
            contents.clear();
        }
    }
    CloseHandle(file);
    return contents;
}

const std::string& injection_library() {
    if (!g_library_loaded) {
        g_library_loaded = true;
        std::wstring path = module_directory();
        path += L"\\shaders\\gbuffer_inject.hlsl";
        g_library = read_text_file(path.c_str());
        if (g_library.empty()) {
            std::wstring fallback = plugin_root();
            fallback += L"\\shaders\\gbuffer_inject.hlsl";
            g_library = read_text_file(fallback.c_str());
        }
        if (g_library.empty()) {
            log_message(
                "Patch de shader: gbuffer_inject.hlsl nao encontrado; "
                "substituicao de shader do jogo desligada.");
        } else {
            g_library_hash =
                hash_bytes(g_library.data(), g_library.size());
            log_message(
                "Patch de shader: biblioteca de injecao carregada (%zu bytes, "
                "assinatura %016llX).",
                g_library.size(),
                static_cast<unsigned long long>(g_library_hash));
        }
    }
    return g_library;
}

const std::wstring& cache_directory() {
    if (!g_cache_directory_ready) {
        g_cache_directory_ready = true;
        std::wstring path = module_directory();
        path += L"\\cache";
        CreateDirectoryW(path.c_str(), nullptr);
        path += L"\\shaders";
        CreateDirectoryW(path.c_str(), nullptr);
        g_cache_directory = path;
    }
    return g_cache_directory;
}

std::wstring cache_path(std::uint64_t key) {
    wchar_t name[64];
    std::swprintf(name, 64, L"\\%016llX.cso", static_cast<unsigned long long>(key));
    return cache_directory() + name;
}

bool read_cache(std::uint64_t key, std::vector<std::uint8_t>* out) {
    const std::wstring path = cache_path(key);
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size = {};
    bool ok = false;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 32 &&
        size.QuadPart < 4 * 1024 * 1024) {
        out->resize(static_cast<std::size_t>(size.QuadPart));
        DWORD read = 0;
        ok = ReadFile(
                 file,
                 out->data(),
                 static_cast<DWORD>(out->size()),
                 &read,
                 nullptr) != 0 &&
             read == out->size();
        if (!ok) {
            out->clear();
        }
    }
    CloseHandle(file);
    return ok;
}

void write_cache(std::uint64_t key, const std::vector<std::uint8_t>& blob) {
    const std::wstring path = cache_path(key);
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(
        file,
        blob.data(),
        static_cast<DWORD>(blob.size()),
        &written,
        nullptr);
    CloseHandle(file);
}

std::string first_error_line(ID3DBlob* errors) {
    if (errors == nullptr || errors->GetBufferPointer() == nullptr) {
        return std::string();
    }
    const char* text = static_cast<const char*>(errors->GetBufferPointer());
    const char* end = std::strchr(text, '\n');
    return end == nullptr ? std::string(text) : std::string(text, end - text);
}

}  // namespace

bool is_enabled() { return g_enabled.load(std::memory_order_acquire); }

void set_enabled(bool enabled) {
    g_enabled.store(enabled, std::memory_order_release);
}

bool patch_pixel_shader(
    const void* bytecode,
    std::size_t size,
    std::vector<std::uint8_t>* patched) {
    if (!is_enabled() || bytecode == nullptr || size < 32 ||
        patched == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> guard(g_mutex);

    if (injection_library().empty()) {
        return false;
    }

    Container container;
    if (!container.parse(bytecode, size)) {
        return false;
    }

    const std::uint64_t key = container.hash() ^ (g_library_hash * 31u);

    if (g_rejected.find(key) != g_rejected.end()) {
        return false;
    }
    const auto cached = g_memory_cache.find(key);
    if (cached != g_memory_cache.end()) {
        *patched = cached->second;
        ++g_statistics.from_cache;
        return true;
    }

    ++g_statistics.inspected;

    Program program;
    if (!decode_program(
            container.code()->data, container.code()->size, &program)) {
        g_rejected[key] = true;
        ++g_statistics.rejected;
        return false;
    }

    const GBufferInfo info = inspect_gbuffer_shader(container, program);
    if (!info.eligible) {
        g_rejected[key] = true;
        ++g_statistics.rejected;
        return false;
    }

    std::vector<std::uint8_t> blob;
    if (read_cache(key, &blob)) {
        g_memory_cache[key] = blob;
        *patched = blob;
        ++g_statistics.from_cache;
        return true;
    }

    EmitOptions options;
    options.prologue = injection_library();
    options.injection = build_injection_call(info);
    EmitResult emitted;
    if (!emit_hlsl(container, program, options, &emitted)) {
        log_message(
            "Patch de shader %016llX recusado: %s.",
            static_cast<unsigned long long>(container.hash()),
            emitted.failure.c_str());
        g_rejected[key] = true;
        ++g_statistics.rejected;
        return false;
    }
    if (emitted.early_return) {
        g_rejected[key] = true;
        ++g_statistics.rejected;
        return false;
    }

    CompileFunction compile = resolve_compiler();
    if (compile == nullptr) {
        g_rejected[key] = true;
        ++g_statistics.rejected;
        return false;
    }

    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT result = compile(
        emitted.source.data(),
        emitted.source.size(),
        "photorealism_gbuffer",
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        kCompileFlags,
        0,
        &code,
        &errors);
    if (FAILED(result) || code == nullptr) {
        log_message(
            "Patch de shader %016llX nao compilou (0x%08X): %s",
            static_cast<unsigned long long>(container.hash()),
            static_cast<unsigned>(result),
            first_error_line(errors).c_str());
        if (errors != nullptr) {
            errors->Release();
        }
        if (code != nullptr) {
            code->Release();
        }
        g_rejected[key] = true;
        ++g_statistics.failed;
        return false;
    }
    if (errors != nullptr) {
        errors->Release();
    }

    blob.assign(
        static_cast<const std::uint8_t*>(code->GetBufferPointer()),
        static_cast<const std::uint8_t*>(code->GetBufferPointer()) +
            code->GetBufferSize());
    code->Release();

    write_cache(key, blob);
    g_memory_cache[key] = blob;
    *patched = blob;
    ++g_statistics.patched;
    log_message(
        "Shader %016llX trocado (#%u): albedo %s/%s em %s, escala %.0f, "
        "%zu -> %zu bytes.",
        static_cast<unsigned long long>(container.hash()),
        g_statistics.patched,
        info.albedo_texture.c_str(),
        info.albedo_sampler.c_str(),
        info.albedo_uv.c_str(),
        static_cast<double>(info.reflection_scale),
        size,
        blob.size());
    return true;
}

PatchStatistics statistics() {
    std::lock_guard<std::mutex> guard(g_mutex);
    return g_statistics;
}

void log_statistics(const char* phase) {
    const PatchStatistics current = statistics();
    log_message(
        "Patch de shader (%s): inspecionados=%u trocados=%u cache=%u "
        "recusados=%u falhas=%u.",
        phase,
        current.inspected,
        current.patched,
        current.from_cache,
        current.rejected,
        current.failed);
}

}
}
