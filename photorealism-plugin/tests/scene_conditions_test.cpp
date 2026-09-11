#include "../src/scene/condition_model.hpp"
#include "../src/scene/condition_smoother.hpp"

#include <cassert>
#include <cmath>

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
}

int main() {
    const ConditionThresholds T = default_condition_thresholds();
    const ConditionAnchors A = default_condition_anchors();

    {
        ConditionWeights w = compute_condition_weights(57.1f, 0.240f, T);
        assert(w.sun > 0.95f && w.rain < 0.05f && w.night < 0.05f);

        w = compute_condition_weights(76.8f, 0.064f, T);
        assert(w.rain > 0.95f && w.sun < 0.05f && w.night < 0.05f);

        w = compute_condition_weights(3.0f, 0.077f, T);
        assert(w.night > 0.95f && w.rain < 0.05f && w.sun < 0.05f);
    }

    {
        ConditionWeights w = compute_condition_weights(20.7f, 0.198f, T);
        assert(w.night < 0.35f);
        assert(w.sun > w.rain);

        w = compute_condition_weights(70.6f, 0.075f, T);
        assert(w.rain > 0.85f);

        w = compute_condition_weights(5.1f, 0.104f, T);
        assert(w.night > 0.85f);
    }

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

        assert(biggest_step < 60.0f);
    }

    {
        ConditionSmoother s;

        assert(s.update(make(76.8f, 0.064f, 108.7f), 0.5f, 180.0f, T));
        assert(s.primed());
        assert(near(s.saturation(), 0.064f, 1e-6f));
        const ConditionWeights w =
            compute_condition_weights(s.median(), s.saturation(), T);
        assert(w.rain > 0.95f);
    }

    {
        ConditionSmoother s;
        s.update(make(76.8f, 0.064f, 108.7f), 0.5f, 180.0f, T);
        const float before = s.saturation();

        assert(s.update(make(0.0f, 0.30f, 0.0f), 0.5f, 180.0f, T));
        assert(near(s.saturation(), before, 1e-6f));

        SceneFeatures invalid = {};
        assert(s.update(invalid, 0.5f, 180.0f, T));
        assert(near(s.saturation(), before, 1e-6f));
    }

    {
        ConditionSmoother s;
        assert(!s.update(make(0.0f, 0.30f, 0.0f), 0.5f, 180.0f, T));
        assert(!s.primed());
    }

    {
        ConditionSmoother s;
        s.update(make(57.1f, 0.240f, 124.8f), 0.5f, 180.0f, T);
        const float start = s.saturation();
        const float target = 0.064f;
        for (int i = 0; i < 360; ++i) {
            s.update(make(76.8f, target, 108.7f), 0.5f, 180.0f, T);
        }
        const float travelled = (start - s.saturation()) / (start - target);
        assert(travelled > 0.55f && travelled < 0.72f);
    }

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

    // --- 11. A suavizacao nao pode depender da taxa de quadros.
    //
    // Regressao da 0.19.0, encontrada no primeiro log de jogo da 0.19.1.
    // `GetTickCount64` tem resolucao de ~15,6 ms, entao acima de 64 fps uma
    // boa parte dos quadros chega com `elapsed_seconds == 0`. O codigo antigo
    // inicializava alpha em 1.0 e so o reduzia quando havia tempo decorrido --
    // ou seja, todo quadro sem tique dava um SALTO COMPLETO para a amostra do
    // momento, e a suavizacao simplesmente nao existia.
    //
    // Medido antes da correcao: a 120 e a 200 fps a mediana suavizada ia de
    // 52,5 para 0,00 em 30 s, quando com tau=180 s deveria parar em 44,4.
    {
        for (const int fps : {60, 120, 200, 500}) {
            ConditionSmoother smoother;
            SceneFeatures sample = make(52.5f, 0.562f, 120.0f);
            smoother.update(sample, 0.0f, 180.0f, T);

            sample = make(0.0f, 0.020f, 120.0f);
            const double frame_ms = 1000.0 / fps;
            double clock = 0.0;
            long long last_tick = 0;
            for (int frame = 0; frame < fps * 30; ++frame) {
                clock += frame_ms;
                const long long tick =
                    static_cast<long long>(clock / 15.6) * 16;
                const float step =
                    tick > last_tick
                        ? static_cast<float>(tick - last_tick) / 1000.0f
                        : 0.0f;
                last_tick = tick;
                smoother.update(sample, step, 180.0f, T);
            }
            // exp(-30/180) * 52,5 = 44,4. Tolerancia larga porque a cadencia
            // de tiques difere entre as taxas; o que o teste exige e que o
            // resultado NAO dependa do fps.
            assert(smoother.median() > 40.0f);
            assert(smoother.median() < 48.0f);
        }
    }

    // --- 12. Sem tempo decorrido, nada se move. E a raiz do defeito acima.
    {
        ConditionSmoother smoother;
        smoother.update(make(50.0f, 0.300f, 120.0f), 0.0f, 180.0f, T);
        for (int i = 0; i < 1000; ++i) {
            smoother.update(make(0.0f, 0.010f, 120.0f), 0.0f, 180.0f, T);
        }
        assert(near(smoother.median(), 50.0f, 1e-4f));
        assert(near(smoother.saturation(), 0.300f, 1e-6f));
    }

    return 0;
}
