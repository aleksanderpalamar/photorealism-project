#pragma once

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <strings.h>

using HMODULE = void*;

inline int _stricmp(const char* a, const char* b) {
    return ::strcasecmp(a, b);
}

inline FILE* _wfopen(const wchar_t* path, const wchar_t*) {
    char narrow[4096] = {};
    ::wcstombs(narrow, path, sizeof(narrow) - 1);
    return std::fopen(narrow, "rb");
}
