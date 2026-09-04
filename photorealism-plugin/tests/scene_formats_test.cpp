#include "../src/scene_formats.hpp"

#include <cassert>

// Regressao da 0.18.0.
//
// O observador de cena passou a versao inteira DESLIGADO. `build.sh` compilou,
// `validate.sh` passou, o teste de features passou -- e o log de uma sessao de
// jogo tinha 663 mil copias de "formato 90 nao suportado" e zero linhas
// "Cena 0.18.0:". A guarda que existia fixava a LINHA DA CHAMADA, e a chamada
// estava certa; o que estava errado era a tabela de formatos logo depois dela,
// que ficava dentro do .cpp onde nenhum teste alcanca.
//
// Formato 90 e DXGI_FORMAT_B8G8R8A8_TYPELESS, que e o que
// `ensure_frame_resources` cria de proposito para poder pendurar uma SRV sRGB
// na copia da cena. Nao e um formato exotico: e o formato do caminho principal.

using namespace photorealism::scene_formats;

int main() {
    // --- 1. O caso que quebrou. O que o plugin realmente entrega ao
    // observador no ETS2 tem que ser legivel, e a ordem de canal tem que ser
    // BGRA -- ler BGRA como RGBA inverte o eixo quente-frio inteiro.
    assert(is_readable(kB8G8R8A8Typeless));
    assert(is_bgra(kB8G8R8A8Typeless));
    assert(resolve_unorm(kB8G8R8A8Typeless) == kB8G8R8A8Unorm);

    // O backbuffer do ETS2 e 87; a copia dele e 90. Os dois caminhos tem que
    // amostrar no mesmo formato concreto, senao a mesma cena daria dois
    // conjuntos de numeros conforme o caminho de criacao que tivesse vencido.
    assert(resolve_unorm(kB8G8R8A8Unorm) == resolve_unorm(kB8G8R8A8Typeless));

    // --- 2. Toda a familia de 8 bits por canal e legivel, e o TYPELESS de
    // cada uma resolve para a UNORM da mesma familia.
    const unsigned bgra[] = {
        kB8G8R8A8Unorm, kB8G8R8A8UnormSrgb, kB8G8R8A8Typeless};
    for (unsigned format : bgra) {
        assert(is_readable(format));
        assert(is_bgra(format));
        assert(!is_rgba(format));
        assert(resolve_unorm(format) == kB8G8R8A8Unorm);
    }
    const unsigned bgrx[] = {
        kB8G8R8X8Unorm, kB8G8R8X8UnormSrgb, kB8G8R8X8Typeless};
    for (unsigned format : bgrx) {
        assert(is_readable(format));
        assert(is_bgra(format));
        assert(resolve_unorm(format) == kB8G8R8X8Unorm);
    }
    const unsigned rgba[] = {
        kR8G8B8A8Unorm, kR8G8B8A8UnormSrgb, kR8G8B8A8Typeless};
    for (unsigned format : rgba) {
        assert(is_readable(format));
        assert(is_rgba(format));
        assert(!is_bgra(format));
        assert(resolve_unorm(format) == kR8G8B8A8Unorm);
    }

    // --- 3. A resolucao NUNCA devolve _SRGB nem TYPELESS.
    //
    // _SRGB faria o GenerateMips decodificar para linear antes da media; as
    // cinco referencias foram medidas com media no espaco de CODIGO e
    // `compute_scene_features` calcula luma sobre o codigo. Trocar isso
    // deslocaria mediana e faixa contra a tabela que scene_features_test fixa,
    // sem que nada acusasse. TYPELESS nao aceita SRV com descritor nulo nem
    // serve de formato de amostra.
    const unsigned every[] = {
        kR8G8B8A8Typeless, kR8G8B8A8Unorm, kR8G8B8A8UnormSrgb,
        kB8G8R8A8Unorm, kB8G8R8X8Unorm, kB8G8R8A8Typeless,
        kB8G8R8A8UnormSrgb, kB8G8R8X8Typeless, kB8G8R8X8UnormSrgb};
    for (unsigned format : every) {
        const unsigned resolved = resolve_unorm(format);
        assert(resolved != kR8G8B8A8UnormSrgb);
        assert(resolved != kB8G8R8A8UnormSrgb);
        assert(resolved != kB8G8R8X8UnormSrgb);
        assert(resolved != kR8G8B8A8Typeless);
        assert(resolved != kB8G8R8A8Typeless);
        assert(resolved != kB8G8R8X8Typeless);
        assert(is_readable(resolved));
        // Idempotente: resolver duas vezes nao muda nada.
        assert(resolve_unorm(resolved) == resolved);
        // A ordem de canal sobrevive a resolucao.
        assert(is_bgra(format) == is_bgra(resolved));
    }

    // --- 4. Formatos que de fato nao dao para ler continuam recusados. A
    // correcao foi aceitar TYPELESS, nao aceitar qualquer coisa: um
    // R16G16B16A16_FLOAT (10) lido como 4 bytes por texel viraria ruido.
    const unsigned unreadable[] = {0u, 2u, 10u, 24u, 40u, 71u, 95u};
    for (unsigned format : unreadable) {
        assert(!is_readable(format));
    }

    return 0;
}
