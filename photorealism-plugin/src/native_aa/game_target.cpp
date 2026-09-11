#include "game_target.hpp"

#include "path_utils.hpp"

#include <shlobj.h>

#include <cwchar>

namespace photorealism {
namespace native_aa {
namespace {

struct KnownGame {
    const wchar_t* executable;
    const wchar_t* documents_directory;
    const char* name;
};

constexpr KnownGame kKnownGames[] = {
    {L"eurotrucks2.exe", L"\\Euro Truck Simulator 2", "ETS2"},
    {L"amtrucks.exe", L"\\American Truck Simulator", "ATS"},
};

const KnownGame* match_executable(const wchar_t* name) {
    for (const KnownGame& game : kKnownGames) {
        if (_wcsicmp(name, game.executable) == 0) {
            return &game;
        }
    }
    return nullptr;
}

}

bool identify_running_game(GameTarget* target) {
    wchar_t executable[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, executable, MAX_PATH) == 0) {
        return false;
    }
    const wchar_t* separator = std::wcsrchr(executable, L'\\');
    const wchar_t* name =
        separator == nullptr ? executable : separator + 1;

    const KnownGame* game = match_executable(name);
    if (game == nullptr) {
        return false;
    }
    target->name = game->name;
    target->documents_directory = game->documents_directory;
    return true;
}

bool resolve_documents_config(GameTarget* target) {
    wchar_t documents[MAX_PATH] = {};
    const HRESULT result = SHGetFolderPathW(
        nullptr,
        CSIDL_PERSONAL | CSIDL_FLAG_CREATE,
        nullptr,
        SHGFP_TYPE_CURRENT,
        documents);
    const bool located =
        SUCCEEDED(result) &&
        append_path(documents, MAX_PATH, target->documents_directory) &&
        append_path(documents, MAX_PATH, L"\\config.cfg");
    if (!located) {
        return false;
    }
    std::wmemcpy(target->config_path, documents, std::wcslen(documents) + 1);
    return true;
}

}
}
