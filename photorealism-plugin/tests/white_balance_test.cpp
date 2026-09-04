#include <cassert>
#include <cmath>

// Balanco de branco normalizado em luminancia -- 0.18.2.
//
// ATENCAO: este teste ESPELHA `apply_temperature` de
// `shaders/photorealism.hlsl`, porque a funcao vive em HLSL e nao ha aqui um
// executor de shader. Espelhar e o custo aceito; `tools/validate.sh` amarra as
// duas copias com um grep na linha da divisao. Se alguem mexer no shader sem
// mexer aqui, e a guarda do validate que fala.
//
// O defeito que isto impede de voltar: ate a 0.18.1 a funcao devolvia
// `color * balance` cru, e o vetor de balanco carregava exposicao. No perfil
// aprovado a luminancia Rec.709 do balanco e 1,028920 -- +0,0411 EV que
// ninguem pediu, contra um `exposure=-0,030` no cfg. A exposicao efetiva era
// +0,0111, com o SINAL TROCADO em relacao ao que o arquivo dizia.

namespace {

constexpr float kLumaR = 0.2126f;
constexpr float kLumaG = 0.7152f;
constexpr float kLumaB = 0.0722f;

float clamp_value(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

struct Balance {
    float r;
    float g;
    float b;
};

Balance raw_balance(float temperature, float tint) {
    const float shift = clamp_value((temperature - 6500.0f) / 3500.0f, -1.0f, 1.0f);
    const float t = clamp_value(tint, -1.0f, 1.0f);
    return Balance{
        1.0f - 0.08f * shift - 0.05f * t,
        1.0f + 0.10f * t,
        1.0f + 0.10f * shift - 0.05f * t};
}

float luminance(const Balance& b) {
    return kLumaR * b.r + kLumaG * b.g + kLumaB * b.b;
}

Balance normalized_balance(float temperature, float tint) {
    const Balance b = raw_balance(temperature, tint);
    const float l = luminance(b);
    const float d = l > 1e-4f ? l : 1e-4f;
    return Balance{b.r / d, b.g / d, b.b / d};
}

bool near(float v, float target, float tol) {
    return std::fabs(v - target) <= tol;
}

}  // namespace

int main() {
    // --- 1. O caso aprovado: 6400 K, tint 0,50. O bruto carrega +0,0411 EV,
    // o normalizado carrega zero.
    {
        const Balance raw = raw_balance(6400.0f, 0.50f);
        assert(near(raw.r, 0.977286f, 1e-5f));
        assert(near(raw.g, 1.050000f, 1e-5f));
        assert(near(raw.b, 0.972143f, 1e-5f));
        // Este e o numero que estava escondido desde a 0.1.2.
        assert(near(luminance(raw), 1.028920f, 1e-5f));
        assert(near(std::log2(luminance(raw)), 0.041130f, 1e-5f));

        const Balance norm = normalized_balance(6400.0f, 0.50f);
        assert(near(luminance(norm), 1.0f, 1e-6f));
    }

    // --- 2. A propriedade, em TODO o dominio util: o balanco normalizado
    // nunca muda o brilho de um pixel neutro. temperature e grampeada em
    // 3000-9000 no config.cpp e tint em -1..1 no shader.
    {
        for (int ti = -20; ti <= 20; ++ti) {
            for (int tk = 3000; tk <= 9000; tk += 250) {
                const float tint = static_cast<float>(ti) / 20.0f;
                const float temperature = static_cast<float>(tk);
                assert(near(
                    luminance(normalized_balance(temperature, tint)),
                    1.0f, 1e-6f));
            }
        }
    }

    // --- 3. A normalizacao preserva a COR: so o brilho sai. As razoes entre
    // canais tem que sobreviver intactas, senao isto teria virado outro
    // balanco em vez do mesmo balanco sem exposicao.
    {
        for (int ti = -20; ti <= 20; ++ti) {
            const float tint = static_cast<float>(ti) / 20.0f;
            const Balance raw = raw_balance(6400.0f, tint);
            const Balance norm = normalized_balance(6400.0f, tint);
            assert(near(norm.r / norm.g, raw.r / raw.g, 1e-6f));
            assert(near(norm.b / norm.g, raw.b / raw.g, 1e-6f));
        }
    }

    // --- 4. A razao de existir: a deriva de brilho ao varrer tint.
    //
    // A 0.19.0 move tint com o clima. Sem normalizar, a imagem clareia ao
    // ficar esverdeada e escurece ao esfriar, sozinha -- e cor que muda brilho
    // e exatamente o que se le como irreal. G pesa 0,7152 dos tres, entao o
    // eixo verde-magenta e justamente o pior dos dois.
    {
        const float low = std::log2(luminance(raw_balance(6400.0f, 0.0f)));
        const float high = std::log2(luminance(raw_balance(6400.0f, 1.0f)));
        // Medido: +0,0004 a +0,0807 EV, ou seja 5,7% de brilho.
        assert(high - low > 0.075f);
        assert(std::pow(2.0f, high - low) - 1.0f > 0.05f);

        // Normalizado, a mesma varredura nao move nada.
        const float nlow = std::log2(luminance(normalized_balance(6400.0f, 0.0f)));
        const float nhigh = std::log2(luminance(normalized_balance(6400.0f, 1.0f)));
        assert(near(nhigh - nlow, 0.0f, 1e-6f));
    }

    // --- 5. O max() nunca morde no dominio real: a luminancia do balanco
    // bruto fica longe de zero em toda parte. Ele existe so para um perfil
    // futuro absurdo nao virar divisao por zero.
    {
        float smallest = 1e9f;
        for (int ti = -20; ti <= 20; ++ti) {
            for (int tk = 3000; tk <= 9000; tk += 250) {
                const float l = luminance(raw_balance(
                    static_cast<float>(tk), static_cast<float>(ti) / 20.0f));
                if (l < smallest) {
                    smallest = l;
                }
            }
        }
        assert(smallest > 0.9f);
    }

    return 0;
}
