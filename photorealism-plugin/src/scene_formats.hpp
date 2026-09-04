#pragma once

namespace photorealism {
namespace scene_formats {

// Os formatos entram aqui como `unsigned`, nao como DXGI_FORMAT, e o motivo e
// unico: assim este cabecalho compila no teste, em Linux, sem d3d11.h. A
// versao 0.18.0 tinha esta tabela dentro de scene_observer.cpp, onde nenhum
// teste alcanca, e o observador passou a versao inteira desligado sem que uma
// unica guarda caisse. Os valores sao os de dxgiformat.h.

constexpr unsigned kR8G8B8A8Typeless = 0x1bu;   // 27
constexpr unsigned kR8G8B8A8Unorm = 0x1cu;      // 28
constexpr unsigned kR8G8B8A8UnormSrgb = 0x1du;  // 29
constexpr unsigned kB8G8R8A8Unorm = 0x57u;      // 87
constexpr unsigned kB8G8R8X8Unorm = 0x58u;      // 88
constexpr unsigned kB8G8R8A8Typeless = 0x5au;   // 90
constexpr unsigned kB8G8R8A8UnormSrgb = 0x5bu;  // 91
constexpr unsigned kB8G8R8X8Typeless = 0x5cu;   // 92
constexpr unsigned kB8G8R8X8UnormSrgb = 0x5du;  // 93

// Todo o grupo BGRA de 8 bits por canal, TYPELESS incluido.
//
// TYPELESS nao e um caso exotico: e o caso NORMAL. `ensure_frame_resources`
// cria a copia da cena como TYPELESS de proposito, para poder pendurar nela
// uma SRV sRGB. Ou seja, o formato que o observador mais recebe era
// exatamente o que ele recusava.
inline bool is_bgra(unsigned format) {
    return format == kB8G8R8A8Unorm || format == kB8G8R8A8UnormSrgb ||
           format == kB8G8R8A8Typeless || format == kB8G8R8X8Unorm ||
           format == kB8G8R8X8UnormSrgb || format == kB8G8R8X8Typeless;
}

inline bool is_rgba(unsigned format) {
    return format == kR8G8B8A8Unorm || format == kR8G8B8A8UnormSrgb ||
           format == kR8G8B8A8Typeless;
}

inline bool is_readable(unsigned format) {
    return is_bgra(format) || is_rgba(format);
}

// Resolve para a variante UNORM da mesma familia.
//
// UNORM e nao _SRGB, e a escolha importa. A reducao e feita pelo GenerateMips
// do hardware: com uma view UNORM ele faz a media dos codigos de 8 bits como
// numeros, com uma view _SRGB ele decodifica para linear, faz a media e
// recodifica. As cinco referencias foram medidas com media no espaco de
// CODIGO, e `compute_scene_features` calcula luma sobre o codigo. Uma view
// _SRGB aqui deslocaria toda a mediana e a faixa em relacao a tabela que o
// teste fixa, sem que nada acusasse.
//
// Um TYPELESS tambem nao pode receber SRV com descritor nulo, entao esta
// resolucao e o que permite criar a view da piramide.
inline unsigned resolve_unorm(unsigned format) {
    if (format == kB8G8R8X8Unorm || format == kB8G8R8X8UnormSrgb ||
        format == kB8G8R8X8Typeless) {
        return kB8G8R8X8Unorm;
    }
    if (is_bgra(format)) {
        return kB8G8R8A8Unorm;
    }
    return kR8G8B8A8Unorm;
}

}  // namespace scene_formats
}  // namespace photorealism
