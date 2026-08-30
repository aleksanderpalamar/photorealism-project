#include "depth_view_space.hlsli"

// Screen-Space Ray-Traced Global Illumination -- acumulacao temporal 0.13.3.
//
// Tres entry points. PSRtgi marcha os raios e escreve o buffer de GI em meia
// resolucao; PSRtgiTemporal soma esse buffer ao do frame anterior; e
// PSRtgiCompose soma o resultado a cor de cena, antes do grading. Ate a 0.12.1
// so o primeiro existia e o buffer nao alimentava ninguem.
//
// A marcha e geometrica, e nao de passo fixo. Com passo fixo (0.12.1) nada era
// amostrado entre range_min e range_min+passo -- 0,5 a 1,71 m com os valores do
// documento -- e a cabine inteira vive nessa faixa: banco a ~0,5 m, painel e
// GPS a ~0,7 m, para-brisa a ~1 m. O RTGI nao alcancava o interior por
// construcao. A razao geometrica poe seis das doze amostras dentro da cabine
// sem tirar alcance do exterior.
//
// 0.13.2.1: os tres desfechos de nao-acerto passaram a devolver o mesmo termo
// de ambiente. Ate aqui um deles devolvia preto duro, e com sky_ambient=0.0 no
// cfg os tres devolviam. Ver ambient_escape.
//
// 0.13.3: quatro raios por pixel e ruido, e ate aqui esse ruido ia inteiro
// para a tela. PSRtgiTemporal acumula frames; o bilateral espacial da 0.13.4
// ainda falta. Ver o bloco de comentario acima de PSRtgiTemporal.

Texture2D<float4> SceneTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);
Texture2D<float4> RtgiTexture : register(t2);
Texture2D<float4> RtgiHistoryTexture : register(t3);
Texture2D<float> DepthHistoryTexture : register(t4);
SamplerState SceneSampler : register(s0);
SamplerState DepthSampler : register(s1);

cbuffer RtgiBuffer : register(b0)
{
    float2 DepthTexelSize;
    float2 ProjectionScale;

    float NearPlane;
    float RayCount;
    float MaxSteps;
    float RangeMin;

    float RangeMax;
    float GiIntensity;
    float MaxIndirectLuma;
    float SkyAmbient;

    float HitThickness;
    float NormalBias;
    float DebugMode;
    float OutputNeedsSrgbEncode;

    float InputNeedsSrgbDecode;
    float FrameIndex;
    float2 RtgiTexelSize;

    float HistoryWeight;
    float DepthRejection;
    float NormalRejection;
    float ColorRejection;

    float HistoryValid;
    float3 RtgiPadding;
};

// Os mesmos valores de photorealism::rtgi::RtgiDebugMode.
static const float kDebugNormals = 0.0;
static const float kDebugRays = 1.0;
static const float kDebugHitDistance = 2.0;
static const float kDebugRawGi = 3.0;
static const float kDebugTemporalGi = 4.0;
static const float kDebugConfidence = 5.0;
static const float kDebugFinal = 6.0;

// Angulo dourado, em voltas em vez de radianos, porque o azimute entra em
// sample_hemisphere como random.y multiplicado por 2*pi. 1 - 1/phi.
static const float kGoldenAngleTurns = 0.3819660112501051;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float sample_raw_depth(float2 uv)
{
    return saturate(DepthTexture.SampleLevel(
        DepthSampler, saturate(uv), 0.0));
}

// Adaptador: le as cinco amostras nos registradores deste shader e delega a
// matematica para o header compartilhado.
float3 sample_view_normal(
    float2 uv, float raw_center, out float normal_valid)
{
    return reconstruct_view_normal(
        uv,
        DepthTexelSize,
        NearPlane,
        ProjectionScale,
        raw_center,
        sample_raw_depth(uv - float2(DepthTexelSize.x, 0.0)),
        sample_raw_depth(uv + float2(DepthTexelSize.x, 0.0)),
        sample_raw_depth(uv - float2(0.0, DepthTexelSize.y)),
        sample_raw_depth(uv + float2(0.0, DepthTexelSize.y)),
        normal_valid);
}

float4 sample_scene_linear(float2 uv)
{
    float3 color = SceneTexture.SampleLevel(
        SceneSampler, saturate(uv), 0.0).rgb;
    if (InputNeedsSrgbDecode > 0.5)
    {
        color = srgb_to_linear(color);
    }
    return float4(color, 1.0);
}

// Hash por pixel, frame e raio. Os raios precisam variar entre frames -- e
// disso que a acumulacao da 0.13.3 extrai amostras de graca.
//
// Ate a 0.13.2.1 isto era frac(sin(dot(seed, ...)) * 43758.5453) com o frame
// somado dentro do seed. Funciona nos primeiros segundos e degrada depois: o
// argumento do sin cresce com o frame e com a resolucao, e em fp32 o sin de
// argumento grande perde exatamente os bits baixos que o frac usa. O hash ia
// empobrecendo ao longo dos minutos em que a acumulacao deveria estar somando
// amostras novas -- o pior momento possivel para isso acontecer.
//
// PCG em inteiro nao tem esse problema. E exato, e a qualidade e a mesma no
// frame 1 e no frame 60000.
uint pcg_hash(uint value)
{
    uint state = value * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float2 ray_random(uint2 pixel, uint frame, uint ray_index)
{
    uint seed = pcg_hash(
        pixel.x + pcg_hash(pixel.y + pcg_hash(frame * 8u + ray_index)));
    uint second = pcg_hash(seed);
    // 2^-32: leva o uint inteiro para [0,1) sem perder faixa.
    return float2(
        (float)seed * 2.3283064365386963e-10,
        (float)second * 2.3283064365386963e-10);
}

// Base ortonormal a partir da normal, sem branch degenerado: o eixo auxiliar
// escolhido e sempre o menos alinhado com ela.
void build_tangent_basis(float3 normal, out float3 tangent, out float3 bitangent)
{
    float3 helper = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0)
                                          : float3(1.0, 0.0, 0.0);
    tangent = normalize(cross(helper, normal));
    bitangent = cross(normal, tangent);
}

// Amostragem cosine-weighted no hemisferio da normal.
//
// A 0.12.1 usava amostragem uniforme com o peso dot(N, dir) aplicado a mao,
// como o documento da tecnica escreve. Com 4 raios a variancia passa a importar
// e o cosseno no PDF a reduz de graca. Consequencia obrigatoria: o dot()
// explicito SAI de march_ray -- mantido junto com o PDF ele viraria cos², que
// escurece demais os bounces rasantes, que sao justamente os que carregam o
// color bleeding de parede e de painel.
float3 sample_hemisphere(float3 normal, float2 random)
{
    float cos_theta = sqrt(saturate(random.x));
    float sin_theta = sqrt(saturate(1.0 - cos_theta * cos_theta));
    float phi = 6.28318530718 * random.y;

    float3 tangent = 0.0.xxx;
    float3 bitangent = 0.0.xxx;
    build_tangent_basis(normal, tangent, bitangent);
    return normalize(
        tangent * (sin_theta * cos(phi)) +
        bitangent * (sin_theta * sin(phi)) +
        normal * cos_theta);
}

struct RayResult
{
    float3 indirect;
    float confidence;
    float distance_travelled;
    float3 direction;
};

// A radiancia atribuida a um raio que ESCAPOU sem acertar nada.
//
// Ate a 0.13.2 tres dos quatro desfechos de march_ray devolviam preto, e com
// sky_ambient=0.0 no cfg os quatro devolviam. Isso torna o preto a resposta
// padrao para "nao sei", que e o vies errado: um raio que atravessou o alcance
// util sem encontrar superficie nenhuma passou por espaco aberto, e espaco
// aberto e claro.
//
// Custava uma cabine inteira. Toda normal reconstruida e forcada a apontar
// para a camera (ver reconstruct_view_normal), entao o hemisferio do painel e
// o cone entre o painel e o olho do motorista -- ar vazio. Os raios tipicos
// andam para tras e cruzam o plano proximo; os rasantes sobem em direcao ao
// para-brisa e voam a frente da estrada, que esta dezenas de metros adiante, e
// esgotam os passos. Nenhum acerta: os quatro raios devolviam exatamente 0.0,
// e quatro zeros tem media exatamente zero -- preto liso, sem nem o granulado
// que denunciaria o problema.
//
// saturate(direction.y) e um modelo de ceu barato: quem olha para cima ve
// mais. O "cima" e o da camera e inclina com ela, limitacao herdada e ainda
// nao resolvida.
float3 ambient_escape(float3 direction)
{
    return SkyAmbient * saturate(direction.y);
}

// Marcha em view-space e projeta de volta para a tela a cada passo.
//
// Quatro desfechos, e a diferenca entre eles e o que o canal de confianca
// carrega: acertar e saber, terminar no ceu tambem e saber, mas sair da tela e
// justamente NAO saber -- screen-space nao tem a informacao. Marcar isso
// separado e o que permite a acumulacao temporal confiar mais em quem sabe.
RayResult march_ray(float3 origin, float3 direction)
{
    RayResult result;
    result.indirect = 0.0.xxx;
    result.confidence = 0.0;
    result.distance_travelled = 0.0;
    result.direction = direction;

    // Espelha photorealism::rtgi::rtgi_step_ratio. O piso de 0.05 e o mesmo
    // kMinimumRange do header, e existe para range_min=0 nao virar divisao por
    // zero na razao.
    float near_bound = max(RangeMin, 0.05);
    float far_bound = max(RangeMax, near_bound * 1.0001);
    float ratio = max(pow(far_bound / near_bound, 1.0 / max(MaxSteps, 1.0)),
                      1.0001);
    float travelled = near_bound;
    int step_count = (int)MaxSteps;

    [loop]
    for (int index = 0; index < step_count; ++index)
    {
        float previous = travelled;
        travelled *= ratio;

        // A ambiguidade de profundidade que a marcha introduz E o comprimento
        // do passo: nada se sabe sobre o que esta entre duas amostras. Perto
        // isso da ~0,05 m e impede a luz de vazar pelo painel; longe o teto do
        // cfg impede que uma fatia de 5 m aceite qualquer coisa como acerto.
        float thickness = min(travelled - previous, HitThickness);
        float3 position = origin + direction * travelled;
        if (position.z <= NearPlane)
        {
            // Atras do plano proximo nao existe projecao valida. Sai do laco
            // para o escape comum: o raio deixou o volume util sem saber de
            // nada, que e a mesma situacao dos passos esgotados.
            break;
        }

        float2 hit_uv = project_view_position(position, ProjectionScale);
        if (hit_uv.x < 0.0 || hit_uv.x > 1.0 ||
            hit_uv.y < 0.0 || hit_uv.y > 1.0)
        {
            // Saiu da tela: desconhecido, nao vazio.
            result.indirect = ambient_escape(direction);
            result.distance_travelled = travelled;
            return result;
        }

        float raw_hit = DepthTexture.SampleLevel(DepthSampler, hit_uv, 0.0);
        raw_hit = saturate(raw_hit);
        if (raw_hit <= 0.0000001)
        {
            // Ceu de verdade: sabemos que nao ha superficie ali.
            result.indirect = ambient_escape(direction);
            result.confidence = 1.0;
            result.distance_travelled = travelled;
            return result;
        }

        float surface_distance = linearize_reversed_depth(raw_hit, NearPlane);
        float delta = position.z - surface_distance;
        if (delta > 0.0 && delta < thickness)
        {
            // Sem dot() aqui: o peso cosseno ja esta no PDF da amostragem.
            result.indirect = sample_scene_linear(hit_uv).rgb;
            result.confidence = 1.0;
            result.distance_travelled = travelled;
            return result;
        }
    }

    // Escape. Duas entradas: os passos acabaram dentro da tela sempre a frente
    // das superficies testadas, ou a marcha cruzou o plano proximo. Nos dois
    // casos o raio saiu do volume util sem encontrar nada, e nao encontrar nada
    // e ceu aberto, nao breu.
    //
    // A confianca fica em zero de proposito: e o desfecho em que o
    // screen-space admite nao saber, e e disso que a rejeicao temporal da
    // 0.13.3 precisa para confiar menos nestes do que num acerto real.
    result.indirect = ambient_escape(direction);
    result.distance_travelled = travelled;
    return result;
}

// Rejeicao de firefly, aplicada POR RAIO e nao depois da media. Uma unica
// amostra estourada -- um farol, o sol num vidro, a HUD -- domina a media de
// quatro raios e vira um pixel branco piscando. Depois da media nao haveria o
// que proteger: o estrago ja estaria diluido em todos.
float3 clamp_indirect_luma(float3 color)
{
    float luma = dot(color, float3(0.2126, 0.7152, 0.0722));
    float ceiling = max(MaxIndirectLuma, 0.01);
    if (luma > ceiling)
    {
        color *= ceiling / max(luma, 0.000001);
    }
    return color;
}

// RGB = luz indireta media, A = confianca media.
RayResult resolve_ray(float2 uv, float raw_depth, uint2 pixel, uint frame)
{
    RayResult empty;
    empty.indirect = 0.0.xxx;
    empty.confidence = 0.0;
    empty.distance_travelled = 0.0;
    empty.direction = 0.0.xxx;

    if (raw_depth <= 0.0000001)
    {
        // Ceu: sem superficie, nao existe ponto de origem para o raio.
        return empty;
    }

    float normal_valid = 0.0;
    float3 view_normal = sample_view_normal(uv, raw_depth, normal_valid);
    if (normal_valid < 0.5)
    {
        return empty;
    }

    float distance_to_surface = linearize_reversed_depth(raw_depth, NearPlane);
    if (distance_to_surface > RangeMax)
    {
        // Fora do alcance util nao ha bounce que valha os passos.
        return empty;
    }

    float3 position = reconstruct_view_position(
        uv, distance_to_surface, ProjectionScale);
    float3 origin = position + view_normal * NormalBias;

    // O primeiro raio e guardado inteiro porque as debug views `rays` e
    // `hit_distance` sao diagnostico POR RAIO -- promediar direcao e distancia
    // de quatro raios nao produz nada interpretavel. Ja `raw_gi` e
    // `confidence` mostram o acumulado, que e o que de fato e composto.
    int ray_count = clamp((int)RayCount, 1, 8);
    float inverse_count = 1.0 / (float)ray_count;
    RayResult accumulated = empty;
    float3 indirect_sum = 0.0.xxx;
    float confidence_sum = 0.0;

    // Estratificacao dentro do frame, rotacao entre frames. Sem as duas a
    // acumulacao da 0.13.3 converge para a media de amostras mal distribuidas,
    // e continua granulada depois de segundos parado.
    //
    // random.x escolhe a inclinacao: partir [0,1) em ray_count faixas e
    // sortear dentro de cada uma cobre o hemisferio de forma regular ja no
    // primeiro frame, em vez de deixar quatro sorteios livres se amontoarem.
    //
    // random.y escolhe o azimute e ganha o angulo dourado vezes o frame.
    // Frames sucessivos passam a INTERCALAR azimutes em vez de sortear
    // independentes: N raios ao longo de K frames cobrem o hemisferio muito
    // melhor que N*K direcoes aleatorias. E o que faz a media temporal
    // convergir rapido, e nao apenas convergir.
    float frame_rotation = frac((float)frame * kGoldenAngleTurns);

    [loop]
    for (int index = 0; index < ray_count; ++index)
    {
        float2 random = ray_random(pixel, frame, (uint)index);
        random.x = ((float)index + random.x) * inverse_count;
        random.y = frac(random.y + frame_rotation);
        float3 direction = sample_hemisphere(view_normal, random);
        RayResult ray = march_ray(origin, direction);
        if (index == 0)
        {
            accumulated = ray;
        }
        indirect_sum += clamp_indirect_luma(ray.indirect);
        confidence_sum += ray.confidence;
    }

    accumulated.indirect = indirect_sum * inverse_count;
    accumulated.confidence = confidence_sum * inverse_count;
    return accumulated;
}

float3 debug_color_for(uint mode, float2 uv, float raw_depth, RayResult ray)
{
    switch (mode)
    {
        case 0:  // normals
        {
            float normal_valid = 0.0;
            float3 view_normal = sample_view_normal(
                uv, raw_depth, normal_valid);
            return (raw_depth <= 0.0000001 || normal_valid < 0.5)
                ? 0.0.xxx
                : view_normal * 0.5 + 0.5;
        }
        case 1:  // rays
            return ray.direction * 0.5 + 0.5;
        case 2:  // hit_distance
            return (ray.distance_travelled / max(RangeMax, 0.001)).xxx;
        case 5:  // confidence
            return ray.confidence.xxx;
        default:  // raw_gi, e temporal_gi ate a 0.12.3 existir
            return ray.indirect;
    }
}

float4 PSRtgi(VertexOutput input) : SV_Target
{
    float2 uv = saturate(input.uv);
    float raw_depth = sample_raw_depth(uv);
    RayResult ray = resolve_ray(
        uv, raw_depth, (uint2)input.position.xy, (uint)(FrameIndex + 0.5));

    if (DebugMode < kDebugFinal - 0.5)
    {
        float3 debug_color = debug_color_for(
            (uint)(DebugMode + 0.5), uv, raw_depth, ray);
        debug_color = saturate(debug_color);
        if (OutputNeedsSrgbEncode > 0.5)
        {
            debug_color = linear_to_srgb(debug_color);
        }
        return float4(debug_color, 1.0);
    }

    return float4(ray.indirect, ray.confidence);
}

// ---------------------------------------------------------------------------
// Acumulacao temporal -- 0.13.3
// ---------------------------------------------------------------------------
//
// Quatro raios por pixel e ruido: a cada frame cada raio sorteia uma direcao
// nova, acerta ou nao acerta, e o pixel cintila. Somar frames e o que
// transforma quatro amostras por frame em dezenas de amostras efetivas, e e a
// unica coisa que torna quatro raios um orcamento viavel.
//
// NAO ha reprojecao. O plugin roda no Present e nao tem as matrizes de camera
// nem motion vectors, entao a historia e lida no MESMO uv -- exatamente como o
// resolve temporal da 0.10.0 em temporal.hlsl. Normalmente isso seria uma
// limitacao seria. Aqui nao e, e vale entender por que: o alvo desta fase e o
// interior da cabine, e o interior e estavel em espaco de tela POR
// CONSTRUCAO. Painel, volante, bancos e portas nao se movem em relacao a
// camera enquanto o caminhao anda, entao para eles o mesmo-uv nao e uma
// aproximacao -- e a reprojecao certa. O que se move e a cena vista pelo
// para-brisa, e ali as rejeicoes abaixo derrubam a historia em vez de borra-la.
//
// Consequencia aceita: em curva o exterior volta ao ruido da 0.13.2.1. Isso e
// o desenho funcionando, nao regressao.

float sample_raw_depth_history(float2 uv)
{
    return saturate(DepthHistoryTexture.SampleLevel(
        DepthSampler, saturate(uv), 0.0));
}

// Irmao de sample_view_normal, lendo a copia de depth do frame anterior.
float3 sample_view_normal_history(
    float2 uv, float raw_center, out float normal_valid)
{
    return reconstruct_view_normal(
        uv,
        DepthTexelSize,
        NearPlane,
        ProjectionScale,
        raw_center,
        sample_raw_depth_history(uv - float2(DepthTexelSize.x, 0.0)),
        sample_raw_depth_history(uv + float2(DepthTexelSize.x, 0.0)),
        sample_raw_depth_history(uv - float2(0.0, DepthTexelSize.y)),
        sample_raw_depth_history(uv + float2(0.0, DepthTexelSize.y)),
        normal_valid);
}

// Confianca pela diferenca RELATIVA de profundidade: 15 cm de erro a 1 m e
// outra superficie, a 50 m e a mesma.
//
// Mesma forma da funcao homonima de temporal.hlsl, reescrita aqui com o limiar
// como parametro em vez de ler o cbuffer daquele passe. A duplicacao e
// deliberada: compartilhar exigiria mover a funcao para o header comum e
// recompilar um shader calibrado e com hash pinado, e o que se ganharia era
// uma funcao.
float depth_history_confidence(
    float current_raw, float history_raw, float rejection)
{
    bool current_sky = current_raw <= 0.0000001;
    bool history_sky = history_raw <= 0.0000001;
    if (current_sky || history_sky)
    {
        return current_sky && history_sky ? 1.0 : 0.0;
    }

    float current_distance = linearize_reversed_depth(current_raw, NearPlane);
    float history_distance = linearize_reversed_depth(history_raw, NearPlane);
    float relative_difference =
        abs(current_distance - history_distance) /
        max(min(current_distance, history_distance), 0.1);
    return 1.0 - smoothstep(
        rejection,
        max(rejection * 2.0, rejection + 0.0001),
        relative_difference);
}

// A rejeicao que a 0.13.2 nao tinha, e a razao de normal_rejection existir no
// cfg desde entao sem fazer nada.
//
// Duas superficies podem estar a mesma distancia da camera e ainda assim ser
// coisas diferentes: a quina do painel contra o para-brisa logo atras dela e o
// caso tipico dentro da cabine. O depth aceita a troca, a normal nao. Com
// normal_rejection=0.85 a historia cai acima de ~31,8 graus de diferenca.
float normal_history_confidence(
    float2 uv, float current_raw, float rejection)
{
    float current_valid = 0.0;
    float history_valid = 0.0;
    float3 current_normal = sample_view_normal(uv, current_raw, current_valid);
    float3 history_normal = sample_view_normal_history(
        uv, sample_raw_depth_history(uv), history_valid);
    if (current_valid < 0.5 || history_valid < 0.5)
    {
        // Sem normal confiavel de um dos lados nao ha o que comparar. Recusar
        // a historia e o lado seguro: perde-se acumulacao, nao se ganha
        // borrao.
        return 0.0;
    }

    float alignment = saturate(dot(current_normal, history_normal));
    float lower = min(rejection, 0.9998);
    float upper = min(lower + max((1.0 - lower) * 0.5, 0.0001), 0.9999);
    return smoothstep(lower, upper, alignment);
}

float4 PSRtgiTemporal(VertexOutput input) : SV_Target
{
    float2 uv = saturate(input.uv);
    float4 current = RtgiTexture.SampleLevel(SceneSampler, uv, 0.0);
    if (HistoryValid < 0.5)
    {
        return current;
    }

    // Clamp de vizinhanca. E o termo livre de escala, e por isso o que carrega
    // o peso num buffer escuro: a historia so pode viver dentro da faixa que
    // os vizinhos do frame atual ja ocupam. Um bounce que mudou de verdade
    // muda a faixa junto, e a historia velha e cortada sozinha.
    float4 minimum_value = current;
    float4 maximum_value = current;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2(x, y) * RtgiTexelSize;
            float4 neighbour = RtgiTexture.SampleLevel(
                SceneSampler, saturate(uv + offset), 0.0);
            minimum_value = min(minimum_value, neighbour);
            maximum_value = max(maximum_value, neighbour);
        }
    }

    float4 history = RtgiHistoryTexture.SampleLevel(SceneSampler, uv, 0.0);
    float4 clipped_history = clamp(history, minimum_value, maximum_value);

    float current_raw = sample_raw_depth(uv);
    float depth_confidence = depth_history_confidence(
        current_raw, sample_raw_depth_history(uv), DepthRejection);
    float normal_confidence = normal_history_confidence(
        uv, current_raw, NormalRejection);

    float color_difference = length(current.rgb - clipped_history.rgb);
    float color_confidence = 1.0 - smoothstep(
        ColorRejection,
        max(ColorRejection * 2.0, ColorRejection + 0.0001),
        color_difference);

    // Produto, e nao media: qualquer um dos tres dizendo "isto nao e a mesma
    // superficie" basta para descartar a historia. Espelhado em C++ por
    // rtgi_history_alpha, que e onde isso vira teste.
    float accepted = saturate(HistoryWeight) *
        depth_confidence * normal_confidence * color_confidence;

    // O alpha entra no lerp junto com a cor: a confianca acumulada e o que a
    // 0.13.4 vai usar para saber onde o sinal e acerto real e onde e so o
    // ambiente da 0.13.2.1.
    return lerp(current, clipped_history, accepted);
}

// Composicao 0.13.2: a cor de cena somada a luz indireta, antes do grading.
//
// Roda em resolucao cheia lendo o buffer de GI em meia resolucao, com upsample
// bilinear do proprio sampler. Isso borra o GI atraves das bordas de geometria;
// o upsample depth-aware entra junto com o denoiser bilateral da 0.13.4.
//
// A soma acontece em espaco linear e o resultado volta ao espaco da copia de
// cena, para que o grading receba exatamente o que ja recebia -- so que com luz
// indireta somada. E por isso que a composicao e um passe proprio em vez de
// virar mais um trecho de photorealism.hlsl: o grading calibrado nao e tocado.
float4 PSRtgiCompose(VertexOutput input) : SV_Target
{
    float2 uv = saturate(input.uv);
    float4 scene = sample_scene_linear(uv);
    float3 indirect = RtgiTexture.SampleLevel(SceneSampler, uv, 0.0).rgb;

    float3 color = scene.rgb + max(indirect, 0.0.xxx) * GiIntensity;
    color = saturate(color);
    if (OutputNeedsSrgbEncode > 0.5)
    {
        color = linear_to_srgb(color);
    }
    return float4(saturate(color), scene.a);
}
