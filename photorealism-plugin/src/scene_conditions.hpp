#pragma once

#include "scene_features.hpp"

namespace photorealism {

// Adaptacao de cor por condicao -- 0.19.0.
//
// Tudo aqui e matematica pura, sem D3D11, pelo motivo que a 0.18.1 ensinou da
// pior forma: tabela de decisao dentro do .cpp e tabela que nenhum teste
// alcanca, e o modulo sai desligado com o build verde.
//
// Os numeros abaixo foram MEDIDOS no ETS2, em 386 amostras de quatro sessoes,
// a partir de blocos sustentados de 3 minutos ou mais. Registro completo em
// `references/scene-baseline-ets2-0.18.1.md`. Nenhum veio do ATS: a faixa de
// ceu R/B no ETS2 e dez vezes mais larga que a das cinco referencias do ATS, e
// um limiar transportado de la classificaria quase tudo errado.

struct ConditionThresholds {
    // Dia contra noite, pela mediana de luminancia.
    //
    // Medido: noite tem mediana 3,0 (p90 = 5,1) e sol tem 57,1 (p10 = 20,7).
    // O vao entre 5,1 e 20,7 nao tem uma amostra sequer. A transicao fica
    // dentro dele, com folga dos dois lados.
    float daylight_median_low;
    float daylight_median_high;

    // Encoberto contra ceu limpo, pela saturacao.
    //
    // Medido: chuva 0,064 (p90 = 0,075) contra sol 0,240 (p10 = 0,198). O vao
    // de 0,075 a 0,198 e maior que as duas faixas somadas -- e a separacao
    // mais limpa das quatro features, e por isso e ela que decide.
    //
    // Foi tambem a que quase se perdeu: ate a terceira coleta a porta de jogo
    // exigia `saturacao > 0,09`, o que descartava 57 das 60 amostras de chuva.
    // Saturacao e FEATURE, nunca criterio de validade.
    float overcast_saturation_low;
    float overcast_saturation_high;

    // Porta de jogo, so estrutura. Descarta quadro preto, fade e carregamento
    // sem tocar em nenhuma condicao real: nas 386 amostras derrubou 6, todas
    // com media de codigo abaixo de 8.
    float minimum_dynamic_range;
};

struct ConditionAnchors {
    // Temperatura em Kelvin na convencao do shader: MENOR = imagem mais
    // quente. Os valores default sao os fotograficos de sempre -- luz solar
    // direta perto de 5500 K, ceu encoberto e sombra perto de 7500 K -- e nao
    // numeros escolhidos a esmo.
    //
    // tint e o eixo verde-magenta. O perfil unico de ate a 0.18.2 usava 0,50
    // em toda condicao, que e a media de condicoes que nao se parecem, e era a
    // origem do esverdeado constante de que o usuario reclamou duas vezes.
    float sun_temperature;
    float sun_tint;
    float rain_temperature;
    float rain_tint;
    float night_temperature;
    float night_tint;
};

struct ConditionWeights {
    float sun;
    float rain;
    float night;
};

struct ConditionGrade {
    float temperature;
    float tint;
};

namespace conditions_detail {

inline float clamp_unit(float value) {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

inline float smoothstep_value(float edge0, float edge1, float x) {
    if (edge1 <= edge0) {
        return x < edge0 ? 0.0f : 1.0f;
    }
    const float t = clamp_unit((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace conditions_detail

inline ConditionThresholds default_condition_thresholds() {
    ConditionThresholds t = {};
    // A banda e larga de proposito. Com 6->20, colada no vao medido, a
    // saida chegou a andar 1074 K em 30 s ao reproduzir as 386 amostras: o
    // smoothstep e ingreme no meio, entao uma mediana suavizada que atravesse
    // a banda depressa arrasta a cor junto. Abrindo para 3->30 o pior caso
    // cai para 609 K, e a separacao continua limpa porque noite tem p90 = 5,1
    // e sol tem p10 = 20,7.
    t.daylight_median_low = 3.0f;
    t.daylight_median_high = 30.0f;
    t.overcast_saturation_low = 0.09f;
    t.overcast_saturation_high = 0.18f;
    t.minimum_dynamic_range = 20.0f;
    return t;
}

inline ConditionAnchors default_condition_anchors() {
    ConditionAnchors a = {};
    a.sun_temperature = 5900.0f;
    a.sun_tint = 0.44f;
    a.rain_temperature = 7500.0f;
    a.rain_tint = 0.30f;
    a.night_temperature = 7000.0f;
    a.night_tint = 0.34f;
    return a;
}

// Pesos continuos, jamais classe dura.
//
// Medido em jogo: aplicar limiar duro amostra a amostra troca de classe 16
// vezes por hora, e suavizar satura em 5 -- cada troca seria um salto de cor
// visivel. Dois eixos com smoothstep e uma soma que fecha em 1 fazem uma
// amostra no meio do caminho sair no meio do caminho, e nao num dos lados.
inline ConditionWeights compute_condition_weights(
    float median, float saturation, const ConditionThresholds& t) {
    const float daylight = conditions_detail::smoothstep_value(
        t.daylight_median_low, t.daylight_median_high, median);
    // 1 quando dessaturado (encoberto), 0 quando saturado (ceu limpo).
    const float overcast = 1.0f - conditions_detail::smoothstep_value(
        t.overcast_saturation_low, t.overcast_saturation_high, saturation);
    ConditionWeights w = {};
    w.night = 1.0f - daylight;
    w.rain = daylight * overcast;
    w.sun = daylight * (1.0f - overcast);
    return w;
}

inline ConditionGrade blend_condition_grade(
    const ConditionWeights& w, const ConditionAnchors& a) {
    ConditionGrade g = {};
    g.temperature = w.sun * a.sun_temperature + w.rain * a.rain_temperature +
                    w.night * a.night_temperature;
    g.tint = w.sun * a.sun_tint + w.rain * a.rain_tint + w.night * a.night_tint;
    return g;
}

// Suavizacao exponencial sobre as FEATURES, nao sobre a saida.
//
// A constante de tempo default e 150 s. Nao e chute: pelo erro de previsao
// causal, o minimo cai em 1,5 min numa sessao e 4 min noutra, com a curva rasa
// entre as duas, e a mesma janela derruba a piscada de classe de 16 para 7 por
// hora. Abaixo de um minuto domina a camera virando; acima de quatro a
// condicao real e achatada.
//
// Em tempo, e nao em amostras: o observador entrega a cada ~0,5 s mas o
// intervalo e configuravel, e uma media de N amostras mudaria de significado
// junto com ele.
class ConditionSmoother {
  public:
    void reset() {
        primed_ = false;
        median_ = 0.0f;
        saturation_ = 0.0f;
    }

    // Devolve true quando ha estado utilizavel. `elapsed_seconds` e o tempo
    // desde a atualizacao anterior.
    bool update(
        const SceneFeatures& features,
        float elapsed_seconds,
        float tau_seconds,
        const ConditionThresholds& thresholds) {
        // Porta de jogo. Quadro sem estrutura e carregamento, fade ou tela
        // preta -- nao e condicao. Segurar o estado anterior e o certo: a
        // condicao nao mudou enquanto a tela estava preta.
        if (!features.valid ||
            features.dynamic_range <= thresholds.minimum_dynamic_range) {
            return primed_;
        }
        if (!primed_) {
            // Primeira amostra valida entra inteira. Comecar de um default e
            // convergir daria varios minutos de cor errada logo na entrada do
            // jogo, que e justamente quando o usuario esta olhando.
            median_ = features.median;
            saturation_ = features.saturation;
            primed_ = true;
            return true;
        }
        float alpha = 1.0f;
        if (tau_seconds > 0.0f && elapsed_seconds > 0.0f) {
            // alpha = 1 - exp(-dt/tau), aproximado sem <cmath> para manter
            // este cabecalho sem dependencia: dt/tau aqui e sempre pequeno.
            const float ratio = elapsed_seconds / tau_seconds;
            alpha = ratio >= 1.0f ? 1.0f : ratio * (1.0f - 0.5f * ratio);
            alpha = conditions_detail::clamp_unit(alpha);
        }
        median_ += (features.median - median_) * alpha;
        saturation_ += (features.saturation - saturation_) * alpha;
        return true;
    }

    bool primed() const { return primed_; }
    float median() const { return median_; }
    float saturation() const { return saturation_; }

  private:
    bool primed_ = false;
    float median_ = 0.0f;
    float saturation_ = 0.0f;
};

}  // namespace photorealism
