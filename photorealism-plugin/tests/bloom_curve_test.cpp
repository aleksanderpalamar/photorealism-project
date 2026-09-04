#include <cassert>
#include <cmath>

// O limiar de joelho do bloom, espelhado de shaders/bloom.hlsl.
//
// Espelhar tem custo -- as duas copias podem divergir -- e o beneficio e
// maior: o shader nao roda no Linux, e a propriedade que este arquivo prova
// (contribuicao EXATAMENTE zero abaixo do joelho) e a diferenca entre bloom e
// um veu cinza sobre a imagem inteira. Sem isto ela seria uma frase num
// comentario em vez de algo que quebra a build.

double srgb_to_linear(double value) {
    if (value <= 0.04045) {
        return value / 12.92;
    }
    return std::pow((value + 0.055) / 1.055, 2.4);
}

// Fracao da cor que sobrevive ao limiar, para um pixel de brilho `brightness`
// em LINEAR. threshold e knee chegam em sRGB 0-1, como no cfg.
double contribution(double brightness, double threshold_srgb,
                    double knee_srgb) {
    const double threshold = srgb_to_linear(threshold_srgb);
    const double raw = srgb_to_linear(threshold_srgb + knee_srgb) - threshold;
    const double knee = raw < 0.0001 ? 0.0001 : raw;

    double soft = brightness - threshold + knee;
    soft = soft < 0.0 ? 0.0 : (soft > 2.0 * knee ? 2.0 * knee : soft);
    soft = soft * soft / (4.0 * knee);

    const double hard = brightness - threshold;
    const double best = soft > hard ? soft : hard;
    const double result = best / (brightness > 0.0001 ? brightness : 0.0001);
    return result < 0.0 ? 0.0 : result;
}

// Os valores da secao [module.bloom.0.17.0].
//
// O limiar E medido: 0.85 em sRGB e o codigo 217, acima do p95 das cinco
// referencias do ATS (117 a 212). E o que garante que o bloom pegue o disco do
// sol e o topo das nuvens em vez do ceu inteiro -- se ele descer para 0.75, a
// faixa 191-212 entra, e essa faixa E o ceu nas duas capturas de golden hour.
const double kThreshold = 0.85;
const double kKnee = 0.06;

int main() {
    const double threshold_linear = srgb_to_linear(kThreshold);
    const double knee_linear =
        srgb_to_linear(kThreshold + kKnee) - threshold_linear;
    const double glow_starts = threshold_linear - knee_linear;

    // REGRESSAO 0.17.0: o zero exato abaixo do joelho.
    //
    // Esta e A propriedade do limiar. Se um pixel escuro contribuir qualquer
    // coisa, TODO pixel da cena contribui, o blur espalha isso pela tela
    // inteira e o bloom deixa de ser brilho em volta de fontes para virar uma
    // nevoa cinza uniforme -- que e exatamente o artefato que faz um bloom
    // parecer amador. Nao basta ser pequeno; tem que ser zero.
    assert(contribution(0.0, kThreshold, kKnee) == 0.0);
    assert(contribution(glow_starts * 0.5, kThreshold, kKnee) == 0.0);
    assert(contribution(glow_starts - 0.001, kThreshold, kKnee) == 0.0);

    // E o zero vale para a cena inteira abaixo do joelho, nao so perto de
    // zero: o asfalto, a cabine e o ceu de dia comum ficam nesta faixa.
    for (int step = 0; step < 100; ++step) {
        const double brightness = glow_starts * (step / 100.0);
        assert(contribution(brightness, kThreshold, kKnee) == 0.0);
    }

    // Continuidade no joelho. Um corte reto faria o glow PISCAR quando um
    // destaque oscila em volta do limiar -- o farol de um caminhao que se
    // aproxima entraria e sairia inteiro a cada frame.
    const double just_below = contribution(glow_starts + 1e-6, kThreshold, kKnee);
    assert(just_below >= 0.0 && just_below < 0.001);

    // Monotonico: mais luz nunca produz menos glow.
    double previous = 0.0;
    for (int step = 0; step <= 200; ++step) {
        const double brightness = step / 100.0;
        const double current = contribution(brightness, kThreshold, kKnee);
        assert(current >= previous - 1e-9);
        previous = current;
    }

    // Bem acima do limiar a contribuicao converge para (brilho - limiar) /
    // brilho, que e o comportamento de um corte reto. O joelho e uma correcao
    // local, e nao uma mudanca de regime: longe dele a curva tem que ser a
    // curva simples.
    const double high = 4.0;
    const double expected = (high - threshold_linear) / high;
    assert(std::fabs(contribution(high, kThreshold, kKnee) - expected) < 1e-9);

    // A conversao sRGB e o que faz o numero do cfg ser o numero que o olho
    // julga. 0.85 em sRGB e o codigo 217, e em linear fica em 0.6921 -- se
    // alguem trocar a comparacao para linear direto, o limiar efetivo desliza
    // de 217 para o codigo 237, e ai quase nada da cena passa.
    assert(std::fabs(threshold_linear - 0.6921) < 0.001);
    assert(threshold_linear > kThreshold * 0.6);
    assert(threshold_linear < kThreshold);

    // Knee zero ainda e valido: vira corte reto, sem divisao por zero.
    assert(contribution(0.1, kThreshold, 0.0) == 0.0);
    assert(contribution(2.0, kThreshold, 0.0) > 0.0);
    return 0;
}
