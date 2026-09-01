#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace photorealism {

// Descricao estatistica de UM frame da cena, medida antes do grade.
//
// As quatro features existem porque sao as que separam as condicoes. Medidas
// nas cinco referencias, reduzidas pelo mesmo caminho que o observador usa em
// jogo (media de area ate ~80 pixels de largura):
//
//   ref        condicao     ceu R/B  mediana  p90-p10  saturacao
//   23-47-51   anoitecer     0,915     13,1     55,0     0,421
//   23-33-14   encoberto     0,967     21,2    131,2     0,338
//   11-12-25   neblina       1,005     21,0    102,4     0,260
//   11-12-15   sol baixo     1,000     30,3     95,2     0,212
//   15-56-22   dia claro     0,938     43,9    157,7     0,183
//
// Nenhuma das quatro separa sozinha: ceu R/B confunde neblina com sol baixo,
// mediana confunde encoberto com neblina, p90-p10 confunde sol baixo com
// neblina e saturacao confunde sol baixo com dia claro. Juntas, o par
// diferente mais proximo fica a 1,65 desvio no espaco normalizado.
struct SceneFeatures {
    bool valid;
    // Eixo quente-frio. Razao R/B da regiao de ceu.
    float sky_r_over_b;
    // Hora do dia. Mediana da luma em 0-255.
    float median;
    // Veu. p90 menos p10 da luma.
    float dynamic_range;
    // Saturacao media da cena, 0-1.
    float saturation;
    float mean;
    // Quantos pixels a amostra tinha. Serve para saber se a medida vale.
    unsigned sample_pixels;
};

namespace features_detail {

// Luma Rec.709 sobre o CODIGO sRGB, nao sobre linear.
//
// E de proposito. As features classificam condicao, e a percepcao de "escuro"
// e "chapado" acompanha o codigo. Alem disso e a mesma unidade em que as cinco
// referencias foram medidas, entao os numeros do log sao diretamente
// comparaveis com a tabela acima.
inline float luma_code(float r, float g, float b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

inline float percentile_of_sorted(
    const std::vector<float>& sorted, float fraction) {
    if (sorted.empty()) {
        return 0.0f;
    }
    const float position = fraction * static_cast<float>(sorted.size() - 1u);
    const std::size_t low = static_cast<std::size_t>(position);
    const std::size_t high =
        std::min<std::size_t>(low + 1u, sorted.size() - 1u);
    const float weight = position - static_cast<float>(low);
    return sorted[low] * (1.0f - weight) + sorted[high] * weight;
}

}  // namespace features_detail

// Calcula as quatro features sobre um bloco de pixels de 8 bits com quatro
// canais. `bgra` diz se a ordem e B,G,R,A (formato 87, o do backbuffer do
// ETS2) ou R,G,B,A.
inline SceneFeatures compute_scene_features(
    const unsigned char* pixels,
    unsigned width,
    unsigned height,
    unsigned pitch,
    bool bgra) {
    SceneFeatures features = {};
    const unsigned count = width * height;
    if (pixels == nullptr || count == 0u) {
        return features;
    }

    std::vector<float> luma;
    luma.reserve(count);
    double saturation_sum = 0.0;
    double luma_sum = 0.0;

    // Regiao de ceu: os mais claros da METADE SUPERIOR.
    //
    // Nao e "a linha de cima" porque a camera de cabine poe o teto la. Os mais
    // claros de cima sao o para-brisa numa vista interna e o ceu numa externa,
    // que e a mesma superficie iluminante nos dois casos -- e por isso a
    // feature sobrevive a troca de camera, que era o risco do detector.
    const unsigned sky_rows = std::max(height / 2u, 1u);
    std::vector<float> sky_luma;
    std::vector<float> sky_r;
    std::vector<float> sky_b;
    const std::size_t sky_capacity =
        static_cast<std::size_t>(sky_rows) * width;
    sky_luma.reserve(sky_capacity);
    sky_r.reserve(sky_capacity);
    sky_b.reserve(sky_capacity);

    for (unsigned y = 0u; y < height; ++y) {
        const unsigned char* row = pixels + static_cast<std::size_t>(y) * pitch;
        for (unsigned x = 0u; x < width; ++x) {
            const unsigned char* texel = row + static_cast<std::size_t>(x) * 4u;
            const float r = static_cast<float>(bgra ? texel[2] : texel[0]);
            const float g = static_cast<float>(texel[1]);
            const float b = static_cast<float>(bgra ? texel[0] : texel[2]);
            const float value = features_detail::luma_code(r, g, b);
            luma.push_back(value);
            luma_sum += value;
            const float maximum = std::max(r, std::max(g, b));
            const float minimum = std::min(r, std::min(g, b));
            if (maximum > 0.0f) {
                saturation_sum +=
                    static_cast<double>((maximum - minimum) / maximum);
            }
            if (y < sky_rows) {
                sky_luma.push_back(value);
                sky_r.push_back(r);
                sky_b.push_back(b);
            }
        }
    }

    std::vector<float> sorted = luma;
    std::sort(sorted.begin(), sorted.end());

    features.valid = true;
    features.sample_pixels = count;
    features.mean = static_cast<float>(luma_sum / count);
    features.median = features_detail::percentile_of_sorted(sorted, 0.50f);
    features.dynamic_range =
        features_detail::percentile_of_sorted(sorted, 0.90f) -
        features_detail::percentile_of_sorted(sorted, 0.10f);
    features.saturation = static_cast<float>(saturation_sum / count);

    // 1.0 e o neutro do eixo, e e o que fica quando nao ha regiao de ceu
    // utilizavel -- um tunel, por exemplo. Devolver 0 faria a condicao ser
    // lida como "frio extremo" justamente quando nao ha informacao.
    features.sky_r_over_b = 1.0f;
    if (!sky_luma.empty()) {
        std::vector<float> sky_sorted = sky_luma;
        std::sort(sky_sorted.begin(), sky_sorted.end());
        const float cut =
            features_detail::percentile_of_sorted(sky_sorted, 0.90f);
        double sum_r = 0.0;
        double sum_b = 0.0;
        unsigned taken = 0u;
        for (std::size_t i = 0u; i < sky_luma.size(); ++i) {
            if (sky_luma[i] < cut) {
                continue;
            }
            sum_r += sky_r[i];
            sum_b += sky_b[i];
            ++taken;
        }
        if (taken > 0u && sum_b > 1.0) {
            features.sky_r_over_b = static_cast<float>(sum_r / sum_b);
        }
    }
    return features;
}

}  // namespace photorealism
