#include <cassert>
#include <cmath>

// A curva de tom da 0.14.0, verificada contra a medicao que a motivou.
//
// As tres funcoes abaixo espelham photorealism.hlsl. Espelhar tem um custo --
// as duas copias podem divergir -- e o beneficio e maior: o shader nao roda no
// Linux, e sem isto o alvo medido nas referencias seria uma frase num
// documento em vez de um numero que falha a build quando alguem o move.

// Encode sRGB, identico ao linear_to_srgb do shader.
double linear_to_srgb(double value) {
    if (value <= 0.0031308) {
        return value * 12.92;
    }
    return 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
}

double apply_black_lift(double color, double lift) {
    const double floor_value = lift < 0.0 ? 0.0 : (lift > 0.02 ? 0.02 : lift);
    return floor_value + (1.0 - floor_value) * color;
}

double apply_highlight_rolloff(double color, double strength) {
    const double amount =
        strength < 0.0 ? 0.0 : (strength > 1.0 ? 1.0 : strength);
    if (amount <= 0.0) {
        return color;
    }
    const double knee = 1.0 + (0.5 - 1.0) * amount;
    const double headroom = (1.0 - knee) < 0.0001 ? 0.0001 : (1.0 - knee);
    const double excess = color - knee > 0.0 ? color - knee : 0.0;
    const double compressed =
        knee + headroom * (1.0 - std::exp(-excess / headroom));
    return color < compressed ? color : compressed;
}

// Codigo de saida em 0-255 de uma cor linear, como um screenshot registra.
int output_code(double linear) {
    const double clamped = linear < 0.0 ? 0.0 : (linear > 1.0 ? 1.0 : linear);
    return static_cast<int>(linear_to_srgb(clamped) * 255.0 + 0.5);
}

// Os valores aprovados da camada base, espelhados de config.cpp. Aquele
// arquivo e Windows-only (usa _stricmp) e nao linka aqui, entao a igualdade
// entre as duas copias e garantida por guarda em validate.sh, e nao pelo
// compilador.
const double kApprovedBlackLift = 0.0027;
const double kApprovedHighlightRolloff = 0.35;

int main() {
    // REGRESSAO 0.14.0: o piso do preto.
    //
    // As quatro referencias do ATS medidas para esta versao tem o 1% mais
    // escuro entre 8 e 11 de 255 -- nada e esmagado a zero. As tres capturas
    // da 0.13.3 tinham 0, porque o saturate() final corta sem toe nenhum, e
    // era por isso que o painel virava massa preta enquanto o da referencia,
    // MAIS escuro na mediana, deixava ler cada manometro.
    //
    // Este e o unico numero que sozinho separa os dois visuais. Se alguem
    // zerar black_lift numa recalibracao futura, o alvo inteiro se perde em
    // silencio -- e este assert e o que impede isso.
    const int floor_code = output_code(apply_black_lift(0.0, 0.0027));
    assert(floor_code >= 6 && floor_code <= 12);
    assert(floor_code == 9);

    // O lift preserva a ordem: escurecer a entrada nunca clareia a saida.
    assert(apply_black_lift(0.0, 0.0027) < apply_black_lift(0.1, 0.0027));
    // E nao mexe no topo de forma perceptivel.
    assert(output_code(apply_black_lift(1.0, 0.0027)) == 255);

    // Fora de faixa nao vira um piso cinza: o clamp segura em 0.02, que ja e
    // um preto levantado agressivo (cerca de 50 em 255).
    assert(output_code(apply_black_lift(0.0, 5.0)) < 64);
    assert(output_code(apply_black_lift(0.0, -1.0)) == 0);

    // REGRESSAO 0.14.0: o ombro.
    //
    // Forca zero devolve a curva antiga, o que mantem o modulo desligavel.
    assert(apply_highlight_rolloff(2.0, 0.0) == 2.0);

    // Nunca ultrapassa 1.0, para qualquer entrada. A exponencial tem 1.0 como
    // ASSINTOTA, e para entrada suficientemente alta ela chega la em ponto
    // flutuante -- exigir estritamente menor que 1.0 seria exigir do numero
    // algo que a curva nao promete.
    assert(apply_highlight_rolloff(50.0, 0.35) <= 1.0);
    assert(apply_highlight_rolloff(1000.0, 0.35) <= 1.0);

    // O que importa de fato: na faixa que o jogo realmente produz ainda sobra
    // gradacao. A copia de cena e o backbuffer, ja em 0-1; so exposicao e
    // contraste levam acima de 1, e com os valores aprovados o topo fica perto
    // de 1,06. Ali o ombro tem que deixar codigo abaixo de 255, senao nao
    // resolveu nada -- era exatamente esse plato que achatava ceu e capo.
    assert(apply_highlight_rolloff(1.06, 0.35) < 1.0);
    assert(output_code(apply_highlight_rolloff(1.06, 0.35)) < 255);
    assert(output_code(apply_highlight_rolloff(1.0, 0.35)) < 255);

    // Abaixo do joelho a imagem passa intacta: o ombro e para os altos.
    assert(apply_highlight_rolloff(0.2, 0.35) == 0.2);
    // E monotonico -- mais luz nunca produz menos codigo.
    assert(apply_highlight_rolloff(0.9, 0.35) <=
           apply_highlight_rolloff(1.5, 0.35));

    // O valor aprovado e o que cai na faixa medida. Se alguem o mover em
    // config.cpp, a guarda de validate.sh reclama; se mover aqui, este assert
    // reclama.
    assert(output_code(apply_black_lift(0.0, kApprovedBlackLift)) == 9);
    assert(output_code(
               apply_highlight_rolloff(1.06, kApprovedHighlightRolloff)) < 255);
    return 0;
}
