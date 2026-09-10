#include "config.hpp"

#include "runtime.hpp"
#include "scene_conditions.hpp"

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Leitura do cfg, dirigida por TABELA.
//
// Ate a 0.19.0 este arquivo era uma sequencia de cadeias de `else if`: uma por
// secao para achar a chave, mais uma para despachar a secao, mais quarenta
// clamps escritos a mao. Cada parametro novo exigia tocar em cinco lugares
// diferentes, e esquecer um deles nao quebrava nada visivelmente.
//
// Foi assim que a `exposure` da base ficou em -0,09 quando o cfg ja dizia
// -0,0488697: a 0.18.2 mudou o arquivo e nao o default interno, e o fallback
// passou a render 0,041 EV mais escuro que o cfg sem que nada acusasse.
//
// Aqui cada parametro e UMA LINHA de tabela, e essa linha carrega ao mesmo
// tempo o nome no arquivo, o campo em Settings e o campo na camada. Parse e
// composicao leem a mesma linha, entao nao ha como uma saber de um campo que a
// outra ignora.

namespace photorealism {
namespace {

// ---------------------------------------------------------------- estruturas

// Uma camada de calibracao. A base traz valores absolutos; visual e
// rain_overcast trazem deltas somados por cima.
struct CalibrationLayer {
    bool enabled;
    float temperature;
    float exposure;
    float contrast;
    float saturation;
    float vibrance;
    float shadows;
    float highlights;
    float blacks;
    float whites;
    float local_contrast;
    float sharpness;
    float vignette;
    // 0.14.0. black_lift e o piso do preto em linear, highlight_rolloff a
    // forca do ombro, tint o eixo verde-magenta que faltava ao balanco.
    // 0.17.1: o piso virou tres numeros, porque o alvo medido nao e cinza.
    float black_lift_r;
    float black_lift_g;
    float black_lift_b;
    float highlight_rolloff;
    float tint;
};

// Tudo que o cfg pode dizer, antes de virar Settings.
//
// So as tres camadas de cor precisam de armazenamento proprio, porque elas se
// somam. Todo o resto -- depth, SSAO, temporal, bloom, observador, adaptacao --
// nao tem camada nenhuma, entao e lido DIRETO no formato final.
//
// Ate a 0.19.0 esta struct redeclarava um a um os cinquenta e cinco campos de
// Settings, e cada tabela precisava de duas colunas para ligar as duas copias.
// Um campo novo tinha que ser escrito nos dois lugares com o mesmo nome, e nada
// reclamava se a segunda copia ficasse para tras.
struct CalibrationStack {
    CalibrationLayer base;
    CalibrationLayer visual_0_2;
    CalibrationLayer rain_overcast_0_3;
    Settings modules;
};

// ------------------------------------------------------------------ utilidades

float clamp_value(float value, float minimum, float maximum) {
    const float low = value < minimum ? minimum : value;
    return low > maximum ? maximum : low;
}

float to_number(const char* value) {
    return static_cast<float>(std::strtod(value, nullptr));
}

bool parse_bool(const char* value) {
    return _stricmp(value, "true") == 0 || _stricmp(value, "yes") == 0 ||
           std::atoi(value) != 0;
}

char* trim(char* text) {
    while (*text != '\0' && std::isspace(static_cast<unsigned char>(*text))) {
        ++text;
    }
    char* end = text + std::strlen(text);
    while (end > text && std::isspace(static_cast<unsigned char>(end[-1]))) {
        --end;
    }
    *end = '\0';
    return text;
}

// ------------------------------------------------------- ligacao chave->campo

// Ponteiro-para-membro e o que permite uma tabela: a linha guarda ONDE
// escrever, e uma unica funcao percorre a tabela escrevendo. Sem isso, cada
// chave precisa do seu proprio ramo, que era o estado anterior deste arquivo.
template <typename Owner>
struct Field {
    const char* key;
    float Owner::*member;
};

template <typename Owner>
bool assign_field(
    Owner* owner,
    const Field<Owner>* fields,
    std::size_t count,
    const char* key,
    const char* value) {
    for (std::size_t i = 0; i < count; ++i) {
        if (_stricmp(fields[i].key, key) != 0) {
            continue;
        }
        owner->*(fields[i].member) = to_number(value);
        return true;
    }
    return false;
}

// --------------------------------------------------------- parametros de cor

// UMA linha por parametro de cor, e ela serve a tres leitores: o parse das
// camadas, a copia da base e a soma dos deltas.
//
// Antes eram tres listas separadas com os mesmos dezessete nomes, e manter as
// tres em dia era trabalho manual. Um parametro novo aqui aparece nos tres
// lugares de uma vez, ou em nenhum.
struct GradeField {
    const char* key;
    float Settings::*effective;
    float CalibrationLayer::*layer;
};

constexpr GradeField kGradeFields[] = {
    {"temperature", &Settings::temperature, &CalibrationLayer::temperature},
    {"exposure", &Settings::exposure, &CalibrationLayer::exposure},
    {"contrast", &Settings::contrast, &CalibrationLayer::contrast},
    {"saturation", &Settings::saturation, &CalibrationLayer::saturation},
    {"vibrance", &Settings::vibrance, &CalibrationLayer::vibrance},
    {"shadows", &Settings::shadows, &CalibrationLayer::shadows},
    {"highlights", &Settings::highlights, &CalibrationLayer::highlights},
    {"blacks", &Settings::blacks, &CalibrationLayer::blacks},
    {"whites", &Settings::whites, &CalibrationLayer::whites},
    {"local_contrast", &Settings::local_contrast,
     &CalibrationLayer::local_contrast},
    {"sharpness", &Settings::sharpness, &CalibrationLayer::sharpness},
    {"vignette", &Settings::vignette, &CalibrationLayer::vignette},
    {"black_lift_r", &Settings::black_lift_r, &CalibrationLayer::black_lift_r},
    {"black_lift_g", &Settings::black_lift_g, &CalibrationLayer::black_lift_g},
    {"black_lift_b", &Settings::black_lift_b, &CalibrationLayer::black_lift_b},
    {"highlight_rolloff", &Settings::highlight_rolloff,
     &CalibrationLayer::highlight_rolloff},
    {"tint", &Settings::tint, &CalibrationLayer::tint},
};

constexpr std::size_t kGradeFieldCount =
    sizeof(kGradeFields) / sizeof(kGradeFields[0]);

// Nas camadas de delta as chaves terminam em `_delta`. Devolve a chave sem o
// sufixo, no buffer do chamador.
const char* strip_delta_suffix(const char* key, char* buffer, std::size_t size) {
    static constexpr char kSuffix[] = "_delta";
    const std::size_t suffix_length = sizeof(kSuffix) - 1;
    const std::size_t key_length = std::strlen(key);
    const bool has_suffix =
        key_length > suffix_length &&
        _stricmp(key + key_length - suffix_length, kSuffix) == 0;
    const std::size_t copy_length = key_length - suffix_length;
    if (!has_suffix || copy_length >= size) {
        return key;
    }
    std::memcpy(buffer, key, copy_length);
    buffer[copy_length] = '\0';
    return buffer;
}

void apply_layer_setting(
    CalibrationLayer* layer, const char* raw_key, const char* value) {
    if (_stricmp(raw_key, "enabled") == 0) {
        layer->enabled = parse_bool(value);
        return;
    }

    char buffer[64] = {};
    const char* key = strip_delta_suffix(raw_key, buffer, sizeof(buffer));

    // Forma escalar da 0.14.0 ate a 0.17.0: escreve os tres canais com o mesmo
    // valor, que e exatamente o piso acromatico que a 0.17.1 corrige. Um cfg
    // antigo carrega e roda; para chegar no alvo medido ele precisa das tres
    // chaves separadas.
    if (_stricmp(key, "black_lift") == 0) {
        const float number = to_number(value);
        layer->black_lift_r = number;
        layer->black_lift_g = number;
        layer->black_lift_b = number;
        return;
    }

    for (std::size_t i = 0; i < kGradeFieldCount; ++i) {
        if (_stricmp(kGradeFields[i].key, key) != 0) {
            continue;
        }
        layer->*(kGradeFields[i].layer) = to_number(value);
        return;
    }
}

// -------------------------------------------------------- parametros de modulo

// Mesma ideia dos parametros de cor, para o que nao e camada: a linha diz o
// nome no arquivo, o campo em Settings e o campo no stack. Parse e copia leem
// a mesma linha.
struct ModuleField {
    const char* key;
    float Settings::*member;
};

constexpr ModuleField kDepthFields[] = {
    {"near_plane", &Settings::depth_near_plane},
    {"preview_distance", &Settings::depth_preview_distance},
    {"vertical_fov", &Settings::depth_vertical_fov},
};

constexpr ModuleField kSsaoFields[] = {
    {"radius", &Settings::ssao_radius},
    {"intensity", &Settings::ssao_intensity},
    {"bias", &Settings::ssao_bias},
    {"fade_start", &Settings::ssao_fade_start},
    {"fade_end", &Settings::ssao_fade_end},
    {"edge_rejection", &Settings::ssao_edge_rejection},
};

constexpr ModuleField kSsaoRefinementFields[] = {
    {"highlight_start", &Settings::ssao_highlight_start},
    {"highlight_end", &Settings::ssao_highlight_end},
    {"highlight_ao_floor", &Settings::ssao_highlight_ao_floor},
};

constexpr ModuleField kSsaoInteriorFields[] = {
    {"near_start", &Settings::ssao_interior_near_start},
    {"near_end", &Settings::ssao_interior_near_end},
    {"radius", &Settings::ssao_interior_radius},
    {"intensity", &Settings::ssao_interior_intensity},
    {"bias", &Settings::ssao_interior_bias},
    {"edge_rejection", &Settings::ssao_interior_edge_rejection},
};

constexpr ModuleField kTemporalFields[] = {
    {"history_weight", &Settings::temporal_history_weight},
    {"depth_rejection", &Settings::temporal_depth_rejection},
    {"color_rejection", &Settings::temporal_color_rejection},
};

constexpr ModuleField kBloomFields[] = {
    {"threshold", &Settings::bloom_threshold},
    {"knee", &Settings::bloom_knee},
    {"intensity", &Settings::bloom_intensity},
    {"radius", &Settings::bloom_radius},
};

constexpr ModuleField kSceneObserverFields[] = {
    {"interval_frames", &Settings::scene_observer_interval_frames},
    {"log_seconds", &Settings::scene_observer_log_seconds},
};

constexpr ModuleField kConditionFields[] = {
    {"time_constant_seconds", &Settings::condition_time_constant_seconds},
    {"log_seconds", &Settings::condition_log_seconds},
    {"daylight_median_low", &Settings::condition_daylight_median_low},
    {"daylight_median_high", &Settings::condition_daylight_median_high},
    {"overcast_saturation_low", &Settings::condition_overcast_saturation_low},
    {"overcast_saturation_high", &Settings::condition_overcast_saturation_high},
    {"minimum_dynamic_range", &Settings::condition_minimum_dynamic_range},
    {"sun_temperature", &Settings::condition_sun_temperature},
    {"sun_tint", &Settings::condition_sun_tint},
    {"rain_temperature", &Settings::condition_rain_temperature},
    {"rain_tint", &Settings::condition_rain_tint},
    {"night_temperature", &Settings::condition_night_temperature},
    {"night_tint", &Settings::condition_night_tint},
};

// ------------------------------------------------------------ tabela de secoes

// Uma linha por secao do cfg. `layer` so e preenchido nas tres camadas de cor;
// `flag` so nas secoes que aceitam `enabled`.
//
// Antes esta informacao estava espalhada por tres lugares -- um `enum`, uma
// cadeia de `if` que traduzia nome em enum, e outra cadeia que traduzia enum em
// funcao. Uma secao nova exigia acertar os tres, e a 0.19.0 chegou a esquecer
// um `continue` nessa segunda cadeia.
struct SectionSpec {
    const char* name;
    bool Settings::*flag;
    CalibrationLayer CalibrationStack::*layer;
    const ModuleField* fields;
    std::size_t field_count;
};

template <std::size_t N>
constexpr std::size_t count_of(const ModuleField (&)[N]) {
    return N;
}

const SectionSpec kSections[] = {
    {"plugin", &Settings::enabled, nullptr, nullptr, 0},
    {"base.0.1.2", nullptr, &CalibrationStack::base, nullptr, 0},
    {"module.visual.0.2.0", nullptr, &CalibrationStack::visual_0_2, nullptr, 0},
    {"module.rain_overcast.0.3.0", nullptr,
     &CalibrationStack::rain_overcast_0_3, nullptr, 0},
    {"depth.0.6.4", nullptr, nullptr, kDepthFields, count_of(kDepthFields)},
    {"module.ssao.0.7.0", &Settings::ssao_enabled, nullptr,
     kSsaoFields, count_of(kSsaoFields)},
    {"module.ssao_refinement.0.8.0", &Settings::ssao_refinement_enabled,
     nullptr, kSsaoRefinementFields, count_of(kSsaoRefinementFields)},
    {"module.ssao_interior.0.9.0", &Settings::ssao_interior_enabled,
     nullptr, kSsaoInteriorFields, count_of(kSsaoInteriorFields)},
    {"module.temporal.0.10.0", &Settings::temporal_enabled, nullptr,
     kTemporalFields, count_of(kTemporalFields)},
    {"module.bloom.0.17.0", &Settings::bloom_enabled, nullptr,
     kBloomFields, count_of(kBloomFields)},
    {"module.scene_observer.0.18.0", &Settings::scene_observer_enabled,
     nullptr, kSceneObserverFields, count_of(kSceneObserverFields)},
    {"module.condition_adaptation.0.19.0",
     &Settings::condition_adaptation_enabled, nullptr, kConditionFields,
     count_of(kConditionFields)},
};

constexpr std::size_t kSectionCount = sizeof(kSections) / sizeof(kSections[0]);

const SectionSpec* find_section(const char* name) {
    for (std::size_t i = 0; i < kSectionCount; ++i) {
        if (_stricmp(kSections[i].name, name) == 0) {
            return &kSections[i];
        }
    }
    return nullptr;
}

void apply_setting(
    CalibrationStack* stack,
    const SectionSpec* section,
    const char* key,
    const char* value) {
    if (section == nullptr) {
        return;
    }
    if (section->layer != nullptr) {
        apply_layer_setting(&(stack->*(section->layer)), key, value);
        return;
    }
    if (section->flag != nullptr && _stricmp(key, "enabled") == 0) {
        stack->modules.*(section->flag) = parse_bool(value);
        return;
    }
    for (std::size_t i = 0; i < section->field_count; ++i) {
        if (_stricmp(section->fields[i].key, key) != 0) {
            continue;
        }
        stack->modules.*(section->fields[i].member) = to_number(value);
        return;
    }
}

// ----------------------------------------------------------------- referencia

CalibrationLayer reference_base() {
    CalibrationLayer layer = {};
    layer.enabled = true;
    layer.temperature = 6500.0f;
    // 0.18.2. Era -0.09 enquanto o balanco de branco carregava +0,0411 EV
    // escondidos. O shader passou a normalizar o balanco e os +0,0411 vieram
    // para ca. Tem que casar com o cfg, senao o fallback interno renderiza mais
    // escuro que o arquivo -- que foi o que aconteceu ate a 0.19.0.
    layer.exposure = -0.0488697f;
    layer.contrast = 0.98f;
    layer.saturation = 0.95f;
    layer.vibrance = -0.05f;
    layer.shadows = 0.04f;
    layer.highlights = -0.05f;
    // Era -0.01f, e somado aos dois deltas dava -0.06 efetivo -- empurrava os
    // pretos para BAIXO, contra o alvo. 0.05f zera a soma das tres camadas e
    // deixa o piso por conta de black_lift, que e quem sabe fazer isso.
    layer.blacks = 0.05f;
    layer.whites = 0.03f;
    layer.local_contrast = 0.12f;
    layer.sharpness = 0.18f;
    layer.vignette = 0.04f;
    // 0.17.2. O piso do preto, por canal, em linear.
    //
    // A 0.17.1 leu as referencias com o estimador de cauda -- media do 1% mais
    // escuro -- sem corrigir o vies que o grao impoe a ele. A cauda e escolhida
    // ordenando por LUMINANCIA, que e 72% G, entao ruido negativo no canal G e
    // o que faz um pixel entrar na amostra. O resultado e que so o G desce.
    //
    // Medido por injecao: somando grao de desvio 2,1 -- o das referencias -- as
    // capturas limpas do plugin, a cauda se desloca -0,18 / -1,83 / +0,43
    // codigos. R e B quase nao se movem porque pesam 0,21 e 0,07 na luminancia.
    //
    // Corrigidas do vies, as tres referencias de tempo claro tem mediana por
    // canal 3,35/6,53/6,22 em 255, convertida para linear por codigo/255/12,92.
    // As duas de neblina tem piso mais alto e mais azul, e entram como delta na
    // camada rain_overcast em vez de puxarem a base.
    layer.black_lift_r = 0.001017f;
    layer.black_lift_g = 0.001982f;
    layer.black_lift_b = 0.001888f;
    layer.highlight_rolloff = 0.35f;
    layer.tint = 0.35f;
    return layer;
}

CalibrationLayer visual_delta_0_2() {
    CalibrationLayer layer = {};
    layer.enabled = true;
    layer.temperature = -100.0f;
    layer.exposure = 0.05f;
    layer.contrast = 0.08f;
    layer.saturation = 0.03f;
    layer.vibrance = 0.09f;
    layer.shadows = 0.04f;
    layer.highlights = -0.09f;
    layer.blacks = -0.04f;
    layer.whites = 0.01f;
    layer.local_contrast = 0.06f;
    layer.sharpness = 0.04f;
    layer.vignette = -0.005f;
    // Os tres da 0.14.0 entram neutros aqui: a primeira rodada move so a base,
    // para o A/B em jogo ter uma variavel de cada vez.
    layer.black_lift_r = 0.0f;
    layer.black_lift_g = 0.0f;
    layer.black_lift_b = 0.0f;
    layer.highlight_rolloff = 0.0f;
    layer.tint = 0.0f;
    return layer;
}

CalibrationLayer rain_overcast_delta_0_3() {
    CalibrationLayer layer = {};
    layer.enabled = true;
    layer.temperature = 0.0f;
    layer.exposure = 0.01f;
    layer.contrast = 0.01f;
    layer.saturation = -0.01f;
    layer.vibrance = 0.01f;
    layer.shadows = 0.02f;
    layer.highlights = -0.04f;
    layer.blacks = -0.01f;
    layer.whites = 0.04f;
    layer.local_contrast = 0.06f;
    layer.sharpness = -0.02f;
    layer.vignette = -0.005f;
    // 0.17.2. Esta camada esta SEMPRE somada, entao quem tem que cair no alvo e
    // a SOMA e nao a base sozinha. Os tres numeros saem de resolver o lift que
    // POE a cauda no alvo medido, invertendo a etapa afim do shader pixel a
    // pixel nas doze capturas da 0.17.1 e bisseccionando por canal.
    layer.black_lift_r = 0.000381f;
    layer.black_lift_g = 0.000498f;
    layer.black_lift_b = 0.000380f;
    layer.highlight_rolloff = 0.0f;
    // A referencia de tempo encoberto e a mais verde das cinco (G/R = 1,21
    // contra 1,11 da de dia claro), entao esta camada acrescenta tint.
    //
    // 0.19.0: com a adaptacao por condicao ligada, temperature e tint efetivos
    // vem das ancoras e nao daqui. Este valor continua sendo o ponto de partida
    // enquanto nao ha amostra de cena valida.
    layer.tint = 0.15f;
    return layer;
}

CalibrationStack reference_stack() {
    const ConditionThresholds thresholds = default_condition_thresholds();
    const ConditionAnchors anchors = default_condition_anchors();

    CalibrationStack stack = {};
    stack.modules.enabled = true;
    stack.base = reference_base();
    stack.visual_0_2 = visual_delta_0_2();
    stack.rain_overcast_0_3 = rain_overcast_delta_0_3();

    stack.modules.depth_near_plane = 0.1f;
    stack.modules.depth_preview_distance = 50.0f;
    stack.modules.depth_vertical_fov = 60.0f;

    stack.modules.ssao_enabled = true;
    stack.modules.ssao_radius = 0.8f;
    stack.modules.ssao_intensity = 0.28f;
    stack.modules.ssao_bias = 0.04f;
    stack.modules.ssao_fade_start = 30.0f;
    stack.modules.ssao_fade_end = 70.0f;
    stack.modules.ssao_edge_rejection = 1.5f;

    stack.modules.ssao_refinement_enabled = true;
    stack.modules.ssao_highlight_start = 0.55f;
    stack.modules.ssao_highlight_end = 0.95f;
    stack.modules.ssao_highlight_ao_floor = 0.35f;

    stack.modules.ssao_interior_enabled = true;
    stack.modules.ssao_interior_near_start = 2.0f;
    stack.modules.ssao_interior_near_end = 8.0f;
    stack.modules.ssao_interior_radius = 0.45f;
    stack.modules.ssao_interior_intensity = 0.20f;
    stack.modules.ssao_interior_bias = 0.05f;
    stack.modules.ssao_interior_edge_rejection = 1.75f;

    stack.modules.temporal_enabled = true;
    stack.modules.temporal_history_weight = 0.65f;
    stack.modules.temporal_depth_rejection = 0.02f;
    stack.modules.temporal_color_rejection = 0.08f;

    // Licenca artistica, e nao o alvo medido. As referencias do ATS nao tem
    // bloom: as bordas de alto contraste sao nitidas e o lado escuro nao tem
    // cauda. Destes quatro so o limiar e medido -- 0.85 em sRGB fica acima do
    // p95 das cinco referencias, entao o bloom pega sol, topo de nuvem e realce
    // de capo, e nao o ceu.
    stack.modules.bloom_enabled = true;
    stack.modules.bloom_threshold = 0.85f;
    stack.modules.bloom_knee = 0.06f;
    stack.modules.bloom_intensity = 0.02f;
    stack.modules.bloom_radius = 0.03f;

    // 0.18.0. Ligado por padrao porque nao muda um pixel: o unico efeito e uma
    // linha no log a cada 30s. 30 frames sao ~0,5s; condicao de tempo muda em
    // minutos, entao amostrar mais rapido so gastaria banda.
    stack.modules.scene_observer_enabled = true;
    stack.modules.scene_observer_interval_frames = 30.0f;
    stack.modules.scene_observer_log_seconds = 30.0f;

    // 0.19.0. Limiares medidos em 386 amostras do ETS2; ancoras sao escolha de
    // look. Os defaults vivem em scene_conditions.hpp, junto do detector que os
    // usa, para nao existirem duas copias.
    stack.modules.condition_adaptation_enabled = true;
    stack.modules.condition_time_constant_seconds = 180.0f;
    stack.modules.condition_log_seconds = 30.0f;
    stack.modules.condition_daylight_median_low = thresholds.daylight_median_low;
    stack.modules.condition_daylight_median_high = thresholds.daylight_median_high;
    stack.modules.condition_overcast_saturation_low =
        thresholds.overcast_saturation_low;
    stack.modules.condition_overcast_saturation_high =
        thresholds.overcast_saturation_high;
    stack.modules.condition_minimum_dynamic_range = thresholds.minimum_dynamic_range;
    stack.modules.condition_sun_temperature = anchors.sun_temperature;
    stack.modules.condition_sun_tint = anchors.sun_tint;
    stack.modules.condition_rain_temperature = anchors.rain_temperature;
    stack.modules.condition_rain_tint = anchors.rain_tint;
    stack.modules.condition_night_temperature = anchors.night_temperature;
    stack.modules.condition_night_tint = anchors.night_tint;
    return stack;
}

// ------------------------------------------------------------------- limites

struct Limit {
    float Settings::*member;
    float minimum;
    float maximum;
};

constexpr Limit kLimits[] = {
    {&Settings::temperature, 3000.0f, 9000.0f},
    {&Settings::exposure, -2.0f, 2.0f},
    {&Settings::contrast, 0.5f, 1.5f},
    {&Settings::saturation, 0.0f, 2.0f},
    {&Settings::vibrance, -1.0f, 1.0f},
    {&Settings::shadows, -1.0f, 1.0f},
    {&Settings::highlights, -1.0f, 1.0f},
    {&Settings::blacks, -1.0f, 1.0f},
    {&Settings::whites, -1.0f, 1.0f},
    {&Settings::local_contrast, 0.0f, 1.0f},
    {&Settings::sharpness, 0.0f, 1.0f},
    {&Settings::vignette, 0.0f, 0.5f},
    {&Settings::depth_near_plane, 0.001f, 10.0f},
    {&Settings::depth_preview_distance, 1.0f, 10000.0f},
    {&Settings::depth_vertical_fov, 20.0f, 140.0f},
    {&Settings::ssao_radius, 0.05f, 5.0f},
    {&Settings::ssao_intensity, 0.0f, 1.0f},
    {&Settings::ssao_bias, 0.0f, 0.5f},
    {&Settings::ssao_fade_start, 1.0f, 500.0f},
    {&Settings::ssao_fade_end, 2.0f, 1000.0f},
    {&Settings::ssao_edge_rejection, 1.05f, 4.0f},
    {&Settings::ssao_highlight_start, 0.0f, 2.0f},
    {&Settings::ssao_highlight_end, 0.01f, 4.0f},
    {&Settings::ssao_highlight_ao_floor, 0.0f, 1.0f},
    {&Settings::ssao_interior_near_start, 0.1f, 50.0f},
    {&Settings::ssao_interior_near_end, 0.2f, 100.0f},
    {&Settings::ssao_interior_radius, 0.05f, 5.0f},
    {&Settings::ssao_interior_intensity, 0.0f, 1.0f},
    {&Settings::ssao_interior_bias, 0.0f, 0.5f},
    {&Settings::ssao_interior_edge_rejection, 1.05f, 4.0f},
    {&Settings::temporal_history_weight, 0.0f, 0.95f},
    {&Settings::temporal_depth_rejection, 0.001f, 0.5f},
    {&Settings::temporal_color_rejection, 0.005f, 1.0f},
    // O teto do limiar fica em 0.98 e nao em 1.0: em 1.0 nada da cena passa e o
    // modulo fica ligado sem produzir nada, que e pior que desligado porque o
    // log diz "ativo". O piso em 0.2 impede que o bloom vire veu sobre a imagem
    // inteira.
    {&Settings::bloom_threshold, 0.2f, 0.98f},
    {&Settings::bloom_knee, 0.0f, 0.5f},
    {&Settings::bloom_intensity, 0.0f, 1.0f},
    {&Settings::bloom_radius, 0.005f, 0.2f},
    // O piso de 1 frame existe para o valor 0 nao virar cadencia degenerada; o
    // teto de 600 (~10s a 60fps) porque acima disso o observador deixa de
    // acompanhar uma transicao de tempo dentro do jogo.
    {&Settings::scene_observer_interval_frames, 1.0f, 600.0f},
    {&Settings::scene_observer_log_seconds, 0.0f, 3600.0f},
    {&Settings::condition_time_constant_seconds, 1.0f, 1800.0f},
    {&Settings::condition_log_seconds, 0.0f, 3600.0f},
    {&Settings::condition_minimum_dynamic_range, 0.0f, 255.0f},
    // As ancoras usam a mesma faixa que temperature e tint estaticos, senao um
    // cfg editado a mao levaria a adaptacao a um lugar onde o perfil fixo nao
    // pode ir.
    {&Settings::condition_sun_temperature, 3000.0f, 9000.0f},
    {&Settings::condition_rain_temperature, 3000.0f, 9000.0f},
    {&Settings::condition_night_temperature, 3000.0f, 9000.0f},
    {&Settings::condition_sun_tint, -1.0f, 1.0f},
    {&Settings::condition_rain_tint, -1.0f, 1.0f},
    {&Settings::condition_night_tint, -1.0f, 1.0f},
};

// Pares que precisam ficar em ordem crescente. Invertidos, o smoothstep de
// quem os consome degenera em degrau -- e no caso da banda de condicao isso
// devolveria a classe dura que a 0.19.0 existe para nao ter.
struct OrderedPair {
    float Settings::*low;
    float Settings::*high;
    float minimum_gap;
};

constexpr OrderedPair kOrderedPairs[] = {
    {&Settings::ssao_fade_start, &Settings::ssao_fade_end, 1.0f},
    {&Settings::ssao_highlight_start, &Settings::ssao_highlight_end, 0.01f},
    {&Settings::ssao_interior_near_start, &Settings::ssao_interior_near_end,
     0.1f},
    {&Settings::condition_daylight_median_low,
     &Settings::condition_daylight_median_high, 1.0f},
    {&Settings::condition_overcast_saturation_low,
     &Settings::condition_overcast_saturation_high, 0.01f},
};

void apply_limits(Settings* settings) {
    for (const Limit& limit : kLimits) {
        settings->*(limit.member) =
            clamp_value(settings->*(limit.member), limit.minimum, limit.maximum);
    }
    for (const OrderedPair& pair : kOrderedPairs) {
        if (settings->*(pair.high) > settings->*(pair.low)) {
            continue;
        }
        settings->*(pair.high) = settings->*(pair.low) + pair.minimum_gap;
    }
}

// ---------------------------------------------------------------- composicao

void copy_base_layer(Settings* settings, const CalibrationLayer& layer) {
    for (const GradeField& field : kGradeFields) {
        settings->*(field.effective) = layer.*(field.layer);
    }
}

void add_delta_layer(Settings* settings, const CalibrationLayer& layer) {
    if (!layer.enabled) {
        return;
    }
    for (const GradeField& field : kGradeFields) {
        settings->*(field.effective) += layer.*(field.layer);
    }
}

Settings compose_stack(const CalibrationStack& stack) {
    // Os modulos ja estao no formato final; so as tres camadas de cor precisam
    // ser compostas.
    Settings settings = stack.modules;

    if (stack.base.enabled) {
        copy_base_layer(&settings, stack.base);
    }
    add_delta_layer(&settings, stack.visual_0_2);
    add_delta_layer(&settings, stack.rain_overcast_0_3);

    apply_limits(&settings);
    return settings;
}

// ---------------------------------------------------------------------- log

void log_effective_profile(const Settings& settings) {
    log_message(
        "Perfil efetivo: temperature=%.1f exposure=%.3f contrast=%.3f "
        "saturation=%.3f vibrance=%.3f shadows=%.3f highlights=%.3f "
        "blacks=%.3f whites=%.3f local_contrast=%.3f sharpness=%.3f "
        "vignette=%.3f.",
        settings.temperature, settings.exposure, settings.contrast,
        settings.saturation, settings.vibrance, settings.shadows,
        settings.highlights, settings.blacks, settings.whites,
        settings.local_contrast, settings.sharpness, settings.vignette);
    // Estes cinco ficaram de fora da linha acima desde a 0.14.0. tint e a maior
    // decisao de cor da cadeia -- sozinho responde por 70% do deficit de
    // vermelho que o plugin introduz -- e nao havia como confirmar em runtime
    // qual valor estava rodando.
    log_message(
        "Perfil efetivo (cor): tint=%.3f highlight_rolloff=%.3f "
        "black_lift=%.6f/%.6f/%.6f.",
        settings.tint, settings.highlight_rolloff, settings.black_lift_r,
        settings.black_lift_g, settings.black_lift_b);
}

void log_white_balance(const Settings& settings) {
    // 0.18.2. O vetor de balanco vai para o log com a luminancia que ele
    // carrega, e nao so com os tres fatores.
    //
    // Ate a 0.18.1 esse ganho era invisivel: o cfg dizia exposure=-0,030, o log
    // repetia, e o balanco somava +0,0411 EV por fora, entao a exposicao real
    // era +0,0111 -- com o sinal trocado. O shader agora normaliza, de modo que
    // ganho_luma tem que sair 1,000000. Se um perfil futuro fizer esse numero
    // sair de 1, e porque alguem devolveu exposicao escondida ao balanco.
    const float shift =
        clamp_value((settings.temperature - 6500.0f) / 3500.0f, -1.0f, 1.0f);
    const float tint = clamp_value(settings.tint, -1.0f, 1.0f);
    const float r = 1.0f - 0.08f * shift - 0.05f * tint;
    const float g = 1.0f + 0.10f * tint;
    const float b = 1.0f + 0.10f * shift - 0.05f * tint;
    const float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    const float divisor = luma > 1e-4f ? luma : 1e-4f;
    const float normalized_luma =
        (0.2126f * (r / divisor) + 0.7152f * (g / divisor) +
         0.0722f * (b / divisor));
    log_message(
        "Balanco de branco 0.18.2: bruto=%.4f/%.4f/%.4f luma_bruta=%.6f "
        "(%+.4f EV) normalizado=%.4f/%.4f/%.4f ganho_luma=%.6f (%+.4f EV).",
        static_cast<double>(r), static_cast<double>(g), static_cast<double>(b),
        static_cast<double>(luma), static_cast<double>(std::log2(luma)),
        static_cast<double>(r / divisor), static_cast<double>(g / divisor),
        static_cast<double>(b / divisor),
        static_cast<double>(normalized_luma),
        static_cast<double>(std::log2(normalized_luma)));
}

void log_modules(const Settings& settings) {
    log_message(
        "Depth linearization 0.6.4: reversed_z=sim near_plane=%.4f "
        "preview_distance=%.1f vertical_fov=%.1f.",
        settings.depth_near_plane, settings.depth_preview_distance,
        settings.depth_vertical_fov);
    log_message(
        "Modulo SSAO 0.7.0: %s samples=8 radius=%.3f intensity=%.3f "
        "bias=%.3f fade=%.1f-%.1f edge_rejection=%.2f.",
        settings.ssao_enabled ? "ativo" : "inativo", settings.ssao_radius,
        settings.ssao_intensity, settings.ssao_bias, settings.ssao_fade_start,
        settings.ssao_fade_end, settings.ssao_edge_rejection);
    log_message(
        "Modulo SSAO refinement 0.8.0: %s samples=16 "
        "highlight_protection=%.2f-%.2f ao_floor=%.2f.",
        settings.ssao_refinement_enabled ? "ativo" : "inativo",
        settings.ssao_highlight_start, settings.ssao_highlight_end,
        settings.ssao_highlight_ao_floor);
    log_message(
        "Modulo SSAO interior 0.9.0: %s faixa=%.1f-%.1fm "
        "radius=%.3f intensity=%.3f bias=%.3f edge_rejection=%.2f.",
        settings.ssao_interior_enabled ? "ativo" : "inativo",
        settings.ssao_interior_near_start, settings.ssao_interior_near_end,
        settings.ssao_interior_radius, settings.ssao_interior_intensity,
        settings.ssao_interior_bias, settings.ssao_interior_edge_rejection);
    log_message(
        "Modulo temporal 0.10.0: %s history_weight=%.2f "
        "depth_rejection=%.3f color_rejection=%.3f.",
        settings.temporal_enabled ? "ativo" : "inativo",
        settings.temporal_history_weight, settings.temporal_depth_rejection,
        settings.temporal_color_rejection);
    log_message(
        "Modulo bloom 0.17.0: %s threshold=%.3f knee=%.3f intensity=%.3f "
        "radius=%.4f (licenca artistica; so o limiar e medido).",
        settings.bloom_enabled ? "ativo" : "inativo", settings.bloom_threshold,
        settings.bloom_knee, settings.bloom_intensity, settings.bloom_radius);
    log_message(
        "Modulo observador de cena 0.18.0: %s intervalo=%.0f frames "
        "log=%.0fs (mede o frame pre-grade; nao altera a imagem).",
        settings.scene_observer_enabled ? "ativo" : "inativo",
        settings.scene_observer_interval_frames,
        settings.scene_observer_log_seconds);
    log_message(
        "Modulo adaptacao por condicao 0.19.0: %s tau=%.0fs log=%.0fs "
        "dia=%.1f-%.1f encoberto_sat=%.3f-%.3f porta_faixa=%.1f.",
        settings.condition_adaptation_enabled ? "ativo" : "inativo",
        static_cast<double>(settings.condition_time_constant_seconds),
        static_cast<double>(settings.condition_log_seconds),
        static_cast<double>(settings.condition_daylight_median_low),
        static_cast<double>(settings.condition_daylight_median_high),
        static_cast<double>(settings.condition_overcast_saturation_low),
        static_cast<double>(settings.condition_overcast_saturation_high),
        static_cast<double>(settings.condition_minimum_dynamic_range));
    log_message(
        "Ancoras 0.19.0: sol=%.0fK/%.3f chuva=%.0fK/%.3f noite=%.0fK/%.3f "
        "(perfil fixo era %.0fK/%.3f em toda condicao).",
        static_cast<double>(settings.condition_sun_temperature),
        static_cast<double>(settings.condition_sun_tint),
        static_cast<double>(settings.condition_rain_temperature),
        static_cast<double>(settings.condition_rain_tint),
        static_cast<double>(settings.condition_night_temperature),
        static_cast<double>(settings.condition_night_tint),
        static_cast<double>(settings.temperature),
        static_cast<double>(settings.tint));
}

void log_stack(const CalibrationStack& stack, const Settings& settings) {
    log_message(
        "Camadas cumulativas: base_0.1.2=%s visual_0.2.0=%s "
        "rain_overcast_0.3.0=%s.",
        stack.base.enabled ? "ativa" : "inativa",
        stack.visual_0_2.enabled ? "ativa" : "inativa",
        stack.rain_overcast_0_3.enabled ? "ativa" : "inativa");
    log_effective_profile(settings);
    log_white_balance(settings);
    log_modules(settings);
}

// --------------------------------------------------------------- leitura do cfg

// Devolve a secao quando a linha e um cabecalho `[nome]`, e diz pelo bool se a
// linha era um cabecalho. Uma secao desconhecida devolve nullptr, e as chaves
// dela sao ignoradas sem erro -- e assim que um cfg de versao futura carrega
// numa versao antiga.
bool read_section_header(char* content, const SectionSpec** section) {
    if (*content != '[') {
        return false;
    }
    char* closing = std::strchr(content + 1, ']');
    if (closing == nullptr) {
        *section = nullptr;
        return true;
    }
    *closing = '\0';
    *section = find_section(trim(content + 1));
    return true;
}

// Parte `chave = valor` no lugar. Devolve false quando a linha nao tem `=`.
bool split_key_value(char* content, char** key, char** value) {
    char* separator = std::strchr(content, '=');
    if (separator == nullptr) {
        return false;
    }
    *separator = '\0';
    *key = trim(content);
    *value = trim(separator + 1);
    return true;
}

bool is_ignorable(const char* content) {
    return *content == '\0' || *content == '#' || *content == ';';
}

void read_stack_from_file(FILE* file, CalibrationStack* stack) {
    const SectionSpec* section = nullptr;
    char line[512] = {};
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        char* content = trim(line);
        if (is_ignorable(content)) {
            continue;
        }
        if (read_section_header(content, &section)) {
            continue;
        }
        char* key = nullptr;
        char* value = nullptr;
        if (!split_key_value(content, &key, &value)) {
            continue;
        }
        apply_setting(stack, section, key, value);
    }
}

}  // namespace

Settings default_settings() {
    return compose_stack(reference_stack());
}

bool load_settings(Settings* settings) {
    if (settings == nullptr) {
        return false;
    }

    CalibrationStack stack = reference_stack();
    FILE* file = _wfopen(config_path(), L"rb");
    if (file == nullptr) {
        *settings = compose_stack(stack);
        log_message("Configuracao ausente; usando a pilha cumulativa interna.");
        log_stack(stack, *settings);
        return false;
    }

    read_stack_from_file(file, &stack);
    std::fclose(file);

    *settings = compose_stack(stack);
    log_stack(stack, *settings);
    return true;
}

}  // namespace photorealism
