#pragma once

#include <windows.h>

namespace photorealism {
namespace native_aa {

struct GameTarget {
    const char* name;
    const wchar_t* documents_directory;
    wchar_t config_path[MAX_PATH];
};

bool identify_running_game(GameTarget* target);
bool resolve_documents_config(GameTarget* target);

}
}
