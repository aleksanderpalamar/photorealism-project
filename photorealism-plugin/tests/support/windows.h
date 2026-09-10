#pragma once

// Substituto mínimo de <windows.h> para os testes rodarem em Linux.
//
// config.cpp precisa de tres coisas do Windows: HMODULE na assinatura de
// runtime.hpp, _stricmp para comparar chaves sem diferenciar caixa, e _wfopen
// para abrir o cfg. Nada disso e logica do plugin, entao substituir e seguro --
// o que o teste exercita e o parse, a composicao e os limites, que sao os
// mesmos binarios que rodam no jogo.

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
