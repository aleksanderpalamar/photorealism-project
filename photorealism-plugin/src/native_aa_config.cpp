#include "native_aa_config.hpp"
#include "native_aa_config_text.hpp"

#include <shlobj.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

namespace {

constexpr std::size_t kMaximumConfigBytes = 2u * 1024u * 1024u;

bool append_path(wchar_t* path, std::size_t capacity, const wchar_t* suffix) {
    const std::size_t used = std::wcslen(path);
    const std::size_t added = std::wcslen(suffix);
    if (used + added + 1 > capacity) {
        return false;
    }
    std::wmemcpy(path + used, suffix, added + 1);
    return true;
}

void log_config(HMODULE module, const char* format, ...) {
    wchar_t path[MAX_PATH] = {};
    if (module == nullptr ||
        GetModuleFileNameW(module, path, MAX_PATH) == 0) {
        return;
    }
    wchar_t* separator = std::wcsrchr(path, L'\\');
    if (separator == nullptr) {
        return;
    }
    *separator = L'\0';
    if (!append_path(path, MAX_PATH, L"\\photorealism-plugin")) {
        return;
    }
    CreateDirectoryW(path, nullptr);
    if (!append_path(path, MAX_PATH, L"\\photorealism-aa-config.log")) {
        return;
    }
    char message[1536] = {};
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    SYSTEMTIME time = {};
    GetLocalTime(&time);
    char line[1792] = {};
    const int length = std::snprintf(
        line,
        sizeof(line),
        "[%02u:%02u:%02u.%03u] %s\r\n",
        static_cast<unsigned>(time.wHour),
        static_cast<unsigned>(time.wMinute),
        static_cast<unsigned>(time.wSecond),
        static_cast<unsigned>(time.wMilliseconds),
        message);
    if (length <= 0) {
        return;
    }
    HANDLE file = CreateFileW(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(
        file,
        line,
        static_cast<DWORD>(
            length < static_cast<int>(sizeof(line)) ? length : sizeof(line)),
        &written,
        nullptr);
    CloseHandle(file);
}

bool read_file(const wchar_t* path, std::string* contents) {
    HANDLE file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        size.QuadPart > static_cast<LONGLONG>(kMaximumConfigBytes)) {
        CloseHandle(file);
        return false;
    }
    contents->resize(static_cast<std::size_t>(size.QuadPart));
    DWORD read = 0;
    const bool ok = ReadFile(
        file,
        contents->data(),
        static_cast<DWORD>(contents->size()),
        &read,
        nullptr) != FALSE &&
        static_cast<std::size_t>(read) == contents->size();
    CloseHandle(file);
    return ok;
}

bool write_atomic(const wchar_t* config_path, const std::string& contents) {
    wchar_t temporary[MAX_PATH] = {};
    std::wcsncpy(temporary, config_path, MAX_PATH - 1);
    wchar_t* name = std::wcsrchr(temporary, L'\\');
    if (name == nullptr) {
        return false;
    }
    name[1] = L'\0';
    if (!append_path(temporary, MAX_PATH, L"config.photorealism-aa.tmp")) {
        return false;
    }
    HANDLE file = CreateFileW(
        temporary,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const bool wrote = WriteFile(
        file,
        contents.data(),
        static_cast<DWORD>(contents.size()),
        &written,
        nullptr) != FALSE &&
        static_cast<std::size_t>(written) == contents.size();
    FlushFileBuffers(file);
    CloseHandle(file);
    if (!wrote || !MoveFileExW(
            temporary,
            config_path,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary);
        return false;
    }
    return true;
}

// Ate a 0.12.1 o plugin zerava r_aa, r_taa_tuning, r_taa_luma_sharpen e
// r_taa_modulated_drr_strength, para assumir integralmente o AA. Isso tem um
// efeito colateral: com o TAA nativo desligado, o Prism3D nao precisa ler o
// depth num shader e o cria sem D3D11_BIND_SHADER_RESOURCE. Sem depth
// legivel, SSAO e resolve temporal ficam sem fonte.
//
// A politica agora vem do photorealism-plugin.cfg, para poder ser ajustada
// sem recompilar. Os defaults abaixo valem quando a secao nao existe.
constexpr const char* kNativeAaSection = "native_aa.0.12.2";
constexpr const char* kDefaultAa = "6";
constexpr const char* kDefaultTaaTuning = "0";
constexpr const char* kDefaultTaaSharpen = "1.5";
constexpr const char* kDefaultTaaDrr = "0.0";

struct NativeAaPolicy {
    bool manage;
    std::string aa;
    std::string taa_tuning;
    std::string taa_sharpen;
    std::string taa_drr;
};

std::string policy_value(
    const std::string& plugin_config,
    const char* key,
    const char* fallback) {
    const std::string value = photorealism::aa_config::plugin_config_value(
        plugin_config, kNativeAaSection, key);
    return value == "ausente" ? std::string(fallback) : value;
}

NativeAaPolicy read_native_aa_policy(HMODULE proxy_module) {
    NativeAaPolicy policy = {
        true, kDefaultAa, kDefaultTaaTuning, kDefaultTaaSharpen,
        kDefaultTaaDrr};

    wchar_t plugin_config_path[MAX_PATH] = {};
    if (GetModuleFileNameW(
            proxy_module, plugin_config_path, MAX_PATH) == 0) {
        return policy;
    }
    wchar_t* separator = std::wcsrchr(plugin_config_path, L'\\');
    if (separator == nullptr) {
        return policy;
    }
    separator[1] = L'\0';
    if (!append_path(
            plugin_config_path,
            MAX_PATH,
            L"photorealism-plugin\\photorealism-plugin.cfg")) {
        return policy;
    }

    std::string plugin_config;
    if (!read_file(plugin_config_path, &plugin_config)) {
        return policy;
    }

    const std::string manage = photorealism::aa_config::plugin_config_value(
        plugin_config, kNativeAaSection, "manage");
    policy.manage = manage != "false" && manage != "0";
    policy.aa = policy_value(plugin_config, "r_aa", kDefaultAa);
    policy.taa_tuning =
        policy_value(plugin_config, "r_taa_tuning", kDefaultTaaTuning);
    policy.taa_sharpen =
        policy_value(plugin_config, "r_taa_luma_sharpen", kDefaultTaaSharpen);
    policy.taa_drr = policy_value(
        plugin_config, "r_taa_modulated_drr_strength", kDefaultTaaDrr);
    return policy;
}

}  // namespace

bool configure_native_aa_for_photorealism(HMODULE proxy_module) {
    wchar_t executable[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, executable, MAX_PATH) == 0) {
        return false;
    }
    const wchar_t* name = std::wcsrchr(executable, L'\\');
    name = name == nullptr ? executable : name + 1;
    const wchar_t* game_directory = nullptr;
    const char* game_name = nullptr;
    if (_wcsicmp(name, L"eurotrucks2.exe") == 0) {
        game_directory = L"\\Euro Truck Simulator 2";
        game_name = "ETS2";
    } else if (_wcsicmp(name, L"amtrucks.exe") == 0) {
        game_directory = L"\\American Truck Simulator";
        game_name = "ATS";
    } else {
        return false;
    }

    wchar_t documents[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(
            nullptr,
            CSIDL_PERSONAL | CSIDL_FLAG_CREATE,
            nullptr,
            SHGFP_TYPE_CURRENT,
            documents)) ||
        !append_path(documents, MAX_PATH, game_directory) ||
        !append_path(documents, MAX_PATH, L"\\config.cfg")) {
        log_config(proxy_module, "AA config: %s Documents/config indisponivel.", game_name);
        return true;
    }

    std::string contents;
    if (!read_file(documents, &contents)) {
        log_config(proxy_module, "AA config: %s config.cfg ainda indisponivel; sera tentado na proxima inicializacao.", game_name);
        return true;
    }
    const std::string detected_aa =
        photorealism::aa_config::config_value(contents, "r_aa");
    const std::string taa_tuning =
        photorealism::aa_config::config_value(contents, "r_taa_tuning");
    const std::string taa_sharpen =
        photorealism::aa_config::config_value(
            contents, "r_taa_luma_sharpen");
    const std::string taa_drr =
        photorealism::aa_config::config_value(
            contents, "r_taa_modulated_drr_strength");
    const NativeAaPolicy policy = read_native_aa_policy(proxy_module);
    if (!policy.manage) {
        log_config(
            proxy_module,
            "AA config: game=%s detected r_aa=%s r_taa_tuning=%s "
            "r_taa_luma_sharpen=%s r_taa_modulated_drr_strength=%s; "
            "manage=false no photorealism-plugin.cfg; nenhuma alteracao no "
            "config.cfg do jogo.",
            game_name,
            detected_aa.c_str(),
            taa_tuning.c_str(),
            taa_sharpen.c_str(),
            taa_drr.c_str());
        return true;
    }

    const bool change_aa =
        detected_aa != "ausente" && detected_aa != policy.aa;
    const bool change_tuning =
        taa_tuning != "ausente" && taa_tuning != policy.taa_tuning;
    const bool change_sharpen =
        taa_sharpen != "ausente" && taa_sharpen != policy.taa_sharpen;
    const bool change_drr =
        taa_drr != "ausente" && taa_drr != policy.taa_drr;
    if (!change_aa && !change_tuning && !change_sharpen && !change_drr) {
        log_config(
            proxy_module,
            "AA config: game=%s detected r_aa=%s r_taa_tuning=%s "
            "r_taa_luma_sharpen=%s r_taa_modulated_drr_strength=%s; "
            "ja em conformidade com a politica do photorealism-plugin.cfg; "
            "nenhuma escrita necessaria.",
            game_name,
            detected_aa.c_str(),
            taa_tuning.c_str(),
            taa_sharpen.c_str(),
            taa_drr.c_str());
        return true;
    }
    wchar_t backup[MAX_PATH] = {};
    std::wcsncpy(backup, documents, MAX_PATH - 1);
    wchar_t* backup_name = std::wcsrchr(backup, L'\\');
    if (backup_name == nullptr) {
        return true;
    }
    backup_name[1] = L'\0';
    if (!append_path(
            backup, MAX_PATH, L"config.photorealism-native-aa.backup.cfg") ||
        (!CopyFileW(documents, backup, TRUE) &&
         GetLastError() != ERROR_FILE_EXISTS)) {
        log_config(
            proxy_module,
            "AA config: game=%s backup falhou; nenhuma configuracao AA/TAA "
            "foi alterada.",
            game_name);
        return true;
    }
    const bool applied_aa = change_aa &&
        photorealism::aa_config::set_config_value(
            &contents, "r_aa", policy.aa.c_str());
    const bool applied_tuning =
        change_tuning && photorealism::aa_config::set_config_value(
            &contents, "r_taa_tuning", policy.taa_tuning.c_str());
    const bool applied_sharpen = change_sharpen &&
        photorealism::aa_config::set_config_value(
            &contents, "r_taa_luma_sharpen", policy.taa_sharpen.c_str());
    const bool applied_drr = change_drr &&
        photorealism::aa_config::set_config_value(
            &contents,
            "r_taa_modulated_drr_strength",
            policy.taa_drr.c_str());
    if (!write_atomic(documents, contents)) {
        log_config(proxy_module, "AA config: game=%s escrita atomica falhou; backup preservado.", game_name);
        return true;
    }
    log_config(
        proxy_module,
        "AA config: game=%s detected r_aa=%s r_taa_tuning=%s "
        "r_taa_luma_sharpen=%s r_taa_modulated_drr_strength=%s; applied "
        "r_aa=%s r_taa_tuning=%s r_taa_luma_sharpen=%s "
        "r_taa_modulated_drr_strength=%s backup="
        "config.photorealism-native-aa.backup.cfg "
        "timing=bootstrap-before-dxgi politica=photorealism-plugin.cfg; "
        "o TAA nativo ligado e o que faz o Prism3D expor o depth como shader "
        "resource, de que SSAO e o resolve temporal dependem.",
        game_name,
        detected_aa.c_str(),
        taa_tuning.c_str(),
        taa_sharpen.c_str(),
        taa_drr.c_str(),
        applied_aa ? policy.aa.c_str() : "unchanged-or-absent",
        applied_tuning ? policy.taa_tuning.c_str() : "unchanged-or-absent",
        applied_sharpen ? policy.taa_sharpen.c_str() : "unchanged-or-absent",
        applied_drr ? policy.taa_drr.c_str() : "unchanged-or-absent");
    return true;
}
