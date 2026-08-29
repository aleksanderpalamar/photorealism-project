#include "depth_view_space.hlsli"

// Screen-Space Ray-Traced Global Illumination -- composicao 0.13.2.
//
// Dois entry points. PSRtgi marcha os raios e escreve o buffer de GI em meia
// resolucao; PSRtgiCompose soma esse buffer a cor de cena, antes do grading.
// Ate a 0.12.1 o segundo nao existia e o buffer nao alimentava ninguem.
//
// A marcha e geometrica, e nao de passo fixo. Com passo fixo (0.12.1) nada era
// amostrado entre range_min e range_min+passo -- 0,5 a 1,71 m com os valores do
// documento -- e a cabine inteira vive nessa faixa: banco a ~0,5 m, painel e
// GPS a ~0,7 m, para-brisa a ~1 m. O RTGI nao alcancava o interior por
// construcao. A razao geometrica poe seis das doze amostras dentro da cabine
// sem tirar alcance do exterior.
//
// Com 4 raios e sem denoise ainda ha ruido temporal. A acumulacao da 0.13.3 e
// o bilateral da 0.13.4 sao o que tornam o sinal limpo.

Texture2D<float4> SceneTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);
Texture2D<float4> RtgiTexture : register(t2);
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
    float2 RtgiPadding;
};

// Os mesmos valores de photorealism::rtgi::RtgiDebugMode.
static const float kDebugNormals = 0.0;
static const float kDebugRays = 1.0;
static const float kDebugHitDistance = 2.0;
static const float kDebugRawGi = 3.0;
static const float kDebugTemporalGi = 4.0;
static const float kDebugConfidence = 5.0;
static const float kDebugFinal = 6.0;

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

// Hash barato por pixel e por frame. Os raios precisam variar entre frames --
// e disso que a acumulacao temporal da 0.12.3 vai extrair amostras de graca.
float2 ray_random(float2 uv, float ray_index)
{
    float3 seed = float3(uv * 4096.0, FrameIndex * 7.0 + ray_index * 13.0);
    float a = frac(sin(dot(seed, float3(12.9898, 78.233, 37.719))) * 43758.5453);
    float b = frac(sin(dot(seed, float3(93.9898, 67.345, 21.987))) * 24634.6345);
    return float2(a, b);
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
            // Atras do plano proximo nao existe projecao valida.
            break;
        }

        float2 hit_uv = project_view_position(position, ProjectionScale);
        if (hit_uv.x < 0.0 || hit_uv.x > 1.0 ||
            hit_uv.y < 0.0 || hit_uv.y > 1.0)
        {
            // Saiu da tela: desconhecido, nao vazio.
            result.indirect = SkyAmbient * saturate(direction.y);
            result.distance_travelled = travelled;
            return result;
        }

        float raw_hit = DepthTexture.SampleLevel(DepthSampler, hit_uv, 0.0);
        raw_hit = saturate(raw_hit);
        if (raw_hit <= 0.0000001)
        {
            // Ceu de verdade: sabemos que nao ha superficie ali.
            result.indirect = SkyAmbient * saturate(direction.y);
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

    // Passos esgotados dentro da tela: nada encontrado no alcance util.
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
RayResult resolve_ray(float2 uv, float raw_depth)
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
    RayResult accumulated = empty;
    float3 indirect_sum = 0.0.xxx;
    float confidence_sum = 0.0;

    [loop]
    for (int index = 0; index < ray_count; ++index)
    {
        float3 direction = sample_hemisphere(
            view_normal, ray_random(uv, (float)index));
        RayResult ray = march_ray(origin, direction);
        if (index == 0)
        {
            accumulated = ray;
        }
        indirect_sum += clamp_indirect_luma(ray.indirect);
        confidence_sum += ray.confidence;
    }

    float inverse_count = 1.0 / (float)ray_count;
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
    RayResult ray = resolve_ray(uv, raw_depth);

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
