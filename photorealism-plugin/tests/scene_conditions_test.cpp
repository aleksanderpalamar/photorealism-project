#include "../src/scene_conditions.hpp"

#include <cassert>
#include <cmath>

// Adaptacao de cor por condicao -- 0.19.0.
//
// Os numeros fixados aqui sao os MEDIDOS no ETS2, em 386 amostras de quatro
// sessoes, a partir de blocos sustentados de 3 min ou mais. Nenhum vem do ATS:
// a faixa de ceu R/B no ETS2 e dez vezes mais larga que a das cinco
// referencias, e um limiar transportado classificaria quase tudo errado.

using namespace photorealism;

namespace {

SceneFeatures make(float median, float saturation, float dynamic_range) {
    SceneFeatures f = {};
    f.valid = true;
    f.sky_r_over_b = 1.0f;
    f.median = median;
    f.dynamic_range = dynamic_range;
    f.saturation = saturation;
    f.mean = median;
    f.sample_pixels = 3600u;
    return f;
}

bool near(float v, float t, float tol) { return std::fabs(v - t) <= tol; }

}  // namespace

int main() {
    const ConditionThresholds T = default_condition_thresholds();
    const ConditionAnchors A = default_condition_anchors();

    // --- 1. As tres medianas medidas caem na condicao certa, com folga.
    {
        // SOL: mediana 57,1  saturacao 0,240
        ConditionWeights w = compute_condition_weights(57.1f, 0.240f, T);
        assert(w.sun > 0.95f && w.rain < 0.05f && w.night < 0.05f);
        // CHUVA: mediana 76,8  saturacao 0,064
        w = compute_condition_weights(76.8f, 0.064f, T);
        assert(w.rain > 0.95f && w.sun < 0.05f && w.night < 0.05f);
        // NOITE: mediana 3,0  saturacao 0,077
        w = compute_condition_weights(3.0f, 0.077f, T);
        assert(w.night > 0.95f && w.rain < 0.05f && w.sun < 0.05f);
    }

    // --- 2. As BORDAS medidas, que sao o caso dificil de verdade.
    //
    // p10/p90 dos blocos sustentados. Se a banda fosse escolhida a esmo, e
    // aqui que ela erraria: sol pode ser tao escuro quanto 20,7 de mediana e
    // chuva tao saturada quanto 0,075.
    {
        // Sol no seu decil mais escuro continua sendo dia, nao noite.
        ConditionWeights w = compute_condition_weights(20.7f, 0.198f, T);
        assert(w.night < 0.35f);
        assert(w.sun > w.rain);
        // Chuva no seu decil mais saturado continua sendo chuva.
        w = compute_condition_weights(70.6f, 0.075f, T);
        assert(w.rain > 0.85f);
        // Noite no seu decil mais claro continua sendo noite.
        w = compute_condition_weights(5.1f, 0.104f, T);
        assert(w.night > 0.85f);
    }

    // --- 3. Os pesos somam 1 em todo o dominio. Sem isso a mistura das
    // ancoras deixaria de ser uma interpolacao e a cor iria para fora da
    // faixa configurada sem que nada acusasse.
    {
        for (int m = 0; m <= 255; m += 5) {
            for (int s = 0; s <= 40; ++s) {
                const ConditionWeights w = compute_condition_weights(
                    static_cast<float>(m), static_cast<float>(s) / 100.0f, T);
                assert(near(w.sun + w.rain + w.night, 1.0f, 1e-5f));
                assert(w.sun >= 0.0f && w.rain >= 0.0f && w.night >= 0.0f);
            }
        }
    }

    // --- 4. A saida NUNCA sai da envoltoria das ancoras.
    //
    // Consequencia direta de (3), mas e a propriedade que o usuario ve: seja
    // qual for a cena, temperature fica entre a menor e a maior ancora e nunca
    // inventa uma cor que ninguem configurou.
    {
        float lo_t = 1e9f, hi_t = -1e9f, lo_i = 1e9f, hi_i = -1e9f;
        for (int m = 0; m <= 255; m += 3) {
            for (int s = 0; s <= 40; ++s) {
                const ConditionWeights w = compute_condition_weights(
                    static_cast<float>(m), static_cast<float>(s) / 100.0f, T);
                const ConditionGrade g = blend_condition_grade(w, A);
                lo_t = g.temperature < lo_t ? g.temperature : lo_t;
                hi_t = g.temperature > hi_t ? g.temperature : hi_t;
                lo_i = g.tint < lo_i ? g.tint : lo_i;
                hi_i = g.tint > hi_i ? g.tint : hi_i;
            }
        }
        assert(lo_t >= A.sun_temperature - 1.0f);
        assert(hi_t <= A.rain_temperature + 1.0f);
        assert(lo_i >= A.rain_tint - 0.001f);
        assert(hi_i <= A.sun_tint + 0.001f);
    }

    // --- 5. NENHUMA CLASSE DURA. E a exigencia central, e ela e medida:
    // um limiar duro troca de classe 16 vezes por hora em jogo, e cada troca
    // seria um salto de cor visivel.
    //
    // Varrendo a saturacao em passos de 0,001, a temperatura nao pode dar um
    // degrau. Se alguem trocar o smoothstep por um if, este assert cai.
    {
        float previous = -1.0f;
        float biggest_step = 0.0f;
        for (int s = 0; s <= 400; ++s) {
            const ConditionWeights w = compute_condition_weights(
                60.0f, static_cast<float>(s) / 1000.0f, T);
            const ConditionGrade g = blend_condition_grade(w, A);
            if (previous >= 0.0f) {
                const float step = std::fabs(g.temperature - previous);
                biggest_step = step > biggest_step ? step : biggest_step;
            }
            previous = g.temperature;
        }
        // A faixa inteira entre as ancoras e 1600 K. Um degrau daria 1600 de
        // uma vez; o smoothstep espalha por 90 passos de 0,001.
        assert(biggest_step < 60.0f);
    }

    // --- 6. A porta de jogo e SO ESTRUTURA.
    //
    // Este e o assert que impede a volta do pior erro desta versao: a porta
    // anterior exigia saturacao > 0,09 e descartava 57 das 60 amostras de
    // chuva medidas. Saturacao baixa nao e quadro invalido -- e o que
    // encoberto e chuva parecem.
    {
        ConditionSmoother s;
        // Chuva medida: saturacao 0,064, bem abaixo do limiar antigo.
        assert(s.update(make(76.8f, 0.064f, 108.7f), 0.5f, 180.0f, T));
        assert(s.primed());
        assert(near(s.saturation(), 0.064f, 1e-6f));
        const ConditionWeights w =
            compute_condition_weights(s.median(), s.saturation(), T);
        assert(w.rain > 0.95f);
    }

    // --- 7. Quadro sem estrutura SEGURA o estado, nao o zera.
    //
    // Um carregamento no meio da chuva nao muda o tempo la fora. Medido: um
    // quadro preto devolveu ceu R/B = 1,596, o valor mais quente de uma sessao
    // inteira -- alimentar isso jogaria o grade para o extremo.
    {
        ConditionSmoother s;
        s.update(make(76.8f, 0.064f, 108.7f), 0.5f, 180.0f, T);
        const float before = s.saturation();
        // Quadro preto de carregamento: faixa zero.
        assert(s.update(make(0.0f, 0.30f, 0.0f), 0.5f, 180.0f, T));
        assert(near(s.saturation(), before, 1e-6f));
        // Feature invalida idem.
        SceneFeatures invalid = {};
        assert(s.update(invalid, 0.5f, 180.0f, T));
        assert(near(s.saturation(), before, 1e-6f));
    }

    // --- 8. Sem amostra alguma, nada esta pronto: o chamador tem que poder
    // manter o perfil fixo do cfg em vez de inventar uma condicao.
    {
        ConditionSmoother s;
        assert(!s.update(make(0.0f, 0.30f, 0.0f), 0.5f, 180.0f, T));
        assert(!s.primed());
    }

    // --- 9. A suavizacao tem a constante de tempo que diz ter.
    //
    // Partindo de sol e saltando para chuva, depois de tau a saturacao tem que
    // ter percorrido ~63% do caminho. E o que fixa os 180 s medidos.
    {
        ConditionSmoother s;
        s.update(make(57.1f, 0.240f, 124.8f), 0.5f, 180.0f, T);
        const float start = s.saturation();
        const float target = 0.064f;
        for (int i = 0; i < 360; ++i) {  // 360 passos de 0,5 s = 180 s
            s.update(make(76.8f, target, 108.7f), 0.5f, 180.0f, T);
        }
        const float travelled = (start - s.saturation()) / (start - target);
        assert(travelled > 0.55f && travelled < 0.72f);
    }

    // --- 10. Ancoras iguais devolvem o perfil fixo, seja qual for a cena.
    // E o que garante que desligar a adaptacao no cfg zerando a diferenca
    // entre as ancoras reproduz a 0.18.2 exatamente.
    {
        ConditionAnchors flat = {};
        flat.sun_temperature = flat.rain_temperature = flat.night_temperature = 6400.0f;
        flat.sun_tint = flat.rain_tint = flat.night_tint = 0.50f;
        for (int m = 0; m <= 255; m += 7) {
            for (int s = 0; s <= 40; s += 3) {
                const ConditionWeights w = compute_condition_weights(
                    static_cast<float>(m), static_cast<float>(s) / 100.0f, T);
                const ConditionGrade g = blend_condition_grade(w, flat);
                assert(near(g.temperature, 6400.0f, 0.01f));
                assert(near(g.tint, 0.50f, 1e-5f));
            }
        }
    }

    return 0;
}
