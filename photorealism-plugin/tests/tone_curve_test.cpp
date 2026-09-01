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

// Contraste em torno do pivo, espelhado de photorealism.hlsl desde a 0.17.1.
// Manda 0 para 0 em vez de para negativo, e e monotonico em todo o dominio.
double apply_contrast(double color, double contrast) {
    const double pivot = 0.18;
    const double floored = color < 1e-6 ? 1e-6 : color;
    return pivot * std::pow(floored / pivot, contrast);
}

// A forma da 0.14.0 ate a 0.17.0. Nao roda mais no shader; existe aqui so para
// que o assert abaixo mostre POR QUE ela saiu, em numero e nao em prosa.
double apply_contrast_affine(double color, double contrast) {
    const double pivot = 0.18;
    const double value = (color - pivot) * contrast + pivot;
    return value < 0.0 ? 0.0 : value;
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
// 0.17.1: o piso e por canal, medido no 1% mais escuro das referencias. A
// base leva o piso de tempo claro; o EFETIVO e a soma das tres camadas, que
// estao sempre ligadas, e mira a mediana por canal das cinco.
// 0.17.2: as mesmas referencias, agora corrigidas do vies do grao sobre o
// estimador de cauda.
const double kApprovedBlackLiftR = 0.001017;
const double kApprovedBlackLiftG = 0.001982;
const double kApprovedBlackLiftB = 0.001888;
const double kEffectiveBlackLiftR = 0.001398;
const double kEffectiveBlackLiftG = 0.002480;
const double kEffectiveBlackLiftB = 0.002268;
const double kApprovedContrast = 1.07;
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

    // REGRESSAO 0.17.1: o piso tem COR.
    //
    // O 1% mais escuro das cinco referencias tem R entre 29% e 64% de G, e nas
    // duas de neblina B passa de G. As quatro capturas da 0.17.0 sairam com
    // piso 8/8/8 e 9/9/9 -- R/G e B/G exatamente 1,000 -- porque o lift era um
    // escalar, e escalar e acromatico por construcao. temperature e tint nao
    // corrigem isso: multiplicam a faixa inteira, e o topo ja esta certo.
    assert(output_code(apply_black_lift(0.0, kApprovedBlackLiftR)) == 3);
    assert(output_code(apply_black_lift(0.0, kApprovedBlackLiftG)) == 7);
    assert(output_code(apply_black_lift(0.0, kApprovedBlackLiftB)) == 6);
    // O piso EFETIVO, que e o que a tela mostra: as tres camadas estao sempre
    // somadas, entao a base sozinha nunca roda.
    //
    // REGRESSAO 0.17.2: 5/8/7, e nao mais 4/7/8. A 0.17.1 leu as referencias
    // com o estimador de cauda sem corrigir o vies que o grao impoe a ele.
    // Ordenar por luminancia para achar o 1% mais escuro e ordenar por um peso
    // que e 72% G, entao ruido negativo NO CANAL G e o que faz um pixel entrar
    // na amostra, e so o G desce. Medido por injecao de grao de desvio 2,1 nas
    // capturas limpas: -0,18 / -1,83 / +0,43 codigos.
    //
    // Cada um destes tres esta dentro da faixa por canal das cinco referencias
    // corrigidas -- R de 2,86 a 7,64, G de 6,15 a 9,67, B de 6,03 a 10,78.
    assert(output_code(apply_black_lift(0.0, kEffectiveBlackLiftR)) == 5);
    assert(output_code(apply_black_lift(0.0, kEffectiveBlackLiftG)) == 8);
    assert(output_code(apply_black_lift(0.0, kEffectiveBlackLiftB)) == 7);
    // As razoes das cinco corrigidas: R/G de 0,465 a 0,790, B/G de 0,924 a
    // 1,137. Essas sao razoes de PISO, e o lift e o piso menos o que a cena
    // poe por cima -- cerca de 0,6 codigo em cada canal. Somar a mesma parcela
    // aos tres puxa a razao na direcao de 1,0, entao a razao do LIFT fica mais
    // longe de 1,0 que a do piso que ele produz: B/G 0,915 aqui contra 0,997
    // no piso simulado. A faixa abaixo e a medida alargada por essa parcela, e
    // e por isso que ela nao e igual a de cima.
    assert(kEffectiveBlackLiftR / kEffectiveBlackLiftG > 0.44);
    assert(kEffectiveBlackLiftR / kEffectiveBlackLiftG < 0.80);
    assert(kEffectiveBlackLiftB / kEffectiveBlackLiftG > 0.87);
    assert(kEffectiveBlackLiftB / kEffectiveBlackLiftG < 1.16);
    // R abaixo de G nos dois -- e o que separa o piso medido de um cinza. Se
    // alguem reigualar os tres, isto reclama.
    assert(kApprovedBlackLiftR < kApprovedBlackLiftG);
    assert(kEffectiveBlackLiftR < kEffectiveBlackLiftG);

    // REGRESSAO 0.17.1: o contraste nao pode esmagar a sombra.
    //
    // Ate a 0.17.0 o contraste era uma reta com max(...,0) no fim. Com
    // Contrast acima de 1 ela manda todo valor abaixo de
    // pivot*(Contrast-1)/Contrast para negativo, e o clamp junta o conjunto
    // inteiro no mesmo zero. Com o perfil aprovado esse limiar e
    // 0,18*0,07/1,07 = 0,01178 na entrada DESTE passo -- que, contadas a
    // exposicao e o ganho de sombra que vem antes, corresponde a 0,0147 na
    // entrada da cadeia, ou 32 em 255 no encode: a cabine inteira.
    //
    // Medido nas quatro capturas da 0.17.0: 72 a 90% dos pixels escuros com os
    // tres canais identicos, e 12 a 13 niveis distintos abaixo de 12/255,
    // contra 24 a 31 nas cinco referencias. O black_lift nao tinha como
    // resolver: ele so escolhia QUAL valor a massa esmagada receberia.
    const double affine_crush_limit =
        0.18 * (kApprovedContrast - 1.0) / kApprovedContrast;
    assert(affine_crush_limit > 0.0117 && affine_crush_limit < 0.0118);
    assert(apply_contrast_affine(0.002, kApprovedContrast) == 0.0);
    assert(apply_contrast_affine(0.010, kApprovedContrast) == 0.0);
    assert(apply_contrast_affine(0.002, kApprovedContrast) ==
           apply_contrast_affine(0.010, kApprovedContrast));
    // E logo acima do limiar ela volta a distinguir, o que mostra que o
    // problema era o clamp e nao a inclinacao.
    assert(apply_contrast_affine(0.012, kApprovedContrast) > 0.0);

    // A potencia mantem os dois separados, e nessa ordem.
    assert(apply_contrast(0.002, kApprovedContrast) > 0.0);
    assert(apply_contrast(0.002, kApprovedContrast) <
           apply_contrast(0.010, kApprovedContrast));
    // Depois do lift eles continuam separados no codigo de saida de 8 bits,
    // que e onde a diferenca vira ou nao vira imagem.
    assert(output_code(apply_black_lift(
               apply_contrast(0.002, kApprovedContrast),
               kApprovedBlackLiftG)) <
           output_code(apply_black_lift(
               apply_contrast(0.010, kApprovedContrast),
               kApprovedBlackLiftG)));

    // O pivo continua sendo o pivo, e o preto continua indo para o preto --
    // as duas propriedades que fazem a troca ser uma correcao e nao um novo
    // visual.
    assert(std::fabs(apply_contrast(0.18, kApprovedContrast) - 0.18) < 1e-9);
    assert(apply_contrast(0.0, kApprovedContrast) < 1e-6);

    // Monotonico em toda a faixa: mais luz nunca produz menos codigo.
    for (int i = 1; i < 512; ++i) {
        const double lo = static_cast<double>(i - 1) / 512.0;
        const double hi = static_cast<double>(i) / 512.0;
        assert(apply_contrast(lo, kApprovedContrast) <=
               apply_contrast(hi, kApprovedContrast));
    }

    // O alvo em numero: o degrade de entrada 0-40 em 255 tem que devolver pelo
    // menos 24 codigos distintos, que e o piso do que as referencias mostram
    // abaixo de 12/255 (24 a 31). A 0.17.0 devolvia 10.
    int distinct = 0;
    int previous = -1;
    for (int code = 0; code <= 40; ++code) {
        const double srgb = static_cast<double>(code) / 255.0;
        const double linear = srgb <= 0.04045
                                  ? srgb / 12.92
                                  : std::pow((srgb + 0.055) / 1.055, 2.4);
        const int out = output_code(apply_black_lift(
            apply_contrast(linear, kApprovedContrast), kApprovedBlackLiftG));
        if (out != previous) {
            ++distinct;
            previous = out;
        }
    }
    assert(distinct >= 24);

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
    assert(output_code(
               apply_highlight_rolloff(1.06, kApprovedHighlightRolloff)) < 255);
    return 0;
}
