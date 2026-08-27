#pragma once

#include <cstddef>
#include <cstdint>

// Dispersao de ponteiros usada pelas tabelas de endereco aberto do modulo FSR.
//
// O shadow do rasterizador precisa derivar o mesmo bucket duas vezes -- uma sob
// o lock, para achar a entrada, e outra sem o lock, para marcar o bucket como
// obsoleto -- entao as duas metades tem de compartilhar exatamente esta funcao.
//
// Deliberadamente livre de Win32/D3D para poder ser testada no Linux.
namespace photorealism::fsr {

// capacity precisa ser potencia de dois; o mascaramento final depende disso.
constexpr std::size_t pointer_bucket(
    std::uintptr_t address, std::size_t capacity) {
    std::uintptr_t value = address;
    value >>= 4;
    value ^= value >> 17;
    value *= static_cast<std::uintptr_t>(0x9E3779B185EBCA87ull);
    return static_cast<std::size_t>(value) & (capacity - 1);
}

}  // namespace photorealism::fsr
