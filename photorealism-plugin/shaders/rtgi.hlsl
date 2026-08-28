#include "depth_view_space.hlsli"

// Screen-Space Ray-Traced Global Illumination -- andaime 0.12.0.
//
// Esta versao ainda nao tracou nenhum raio. Ela existe para exercitar o
// caminho completo -- recursos em meia resolucao, bloco constante, compilacao
// e o draw -- com o custo real, sem alterar a imagem. A luz indireta sai
// zerada de proposito: compor zero e uma operacao neutra, entao ligar o modulo
// nesta versao nao pode piorar nada.
//
// A 0.12.1 preenche ray_march_indirect(); o resto do arquivo ja esta no lugar
// que ela vai precisar.

Texture2D<float4> SceneTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);
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

    float DebugMode;
    float OutputNeedsSrgbEncode;
    float FrameIndex;
    float RtgiPadding;
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

// RGB = luz indireta, A = confianca.
//
// Na 0.12.0 nao ha raio: a confianca e a validade da normal, e a luz indireta
// e zero. A 0.12.1 substitui o corpo por ray generation + screen-space ray
// march, mantendo a mesma assinatura.
float4 resolve_indirect(float2 uv, float raw_depth)
{
    if (raw_depth <= 0.0000001)
    {
        // Ceu: sem superficie, nao existe ponto de origem para o raio.
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float normal_valid = 0.0;
    float3 view_normal = sample_view_normal(uv, raw_depth, normal_valid);
    if (normal_valid < 0.5)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float distance_to_surface = linearize_reversed_depth(raw_depth, NearPlane);
    // Fora do alcance util nao ha bounce que valha os passos.
    float range_weight = distance_to_surface <= RangeMax ? 1.0 : 0.0;

    float3 indirect = 0.0.xxx;
    return float4(indirect, range_weight);
}

float4 PSRtgi(VertexOutput input) : SV_Target
{
    float2 uv = saturate(input.uv);
    float raw_depth = sample_raw_depth(uv);
    float4 resolved = resolve_indirect(uv, raw_depth);

    if (DebugMode < kDebugFinal - 0.5)
    {
        float3 debug_color = resolved.rgb;
        if (DebugMode < kDebugNormals + 0.5)
        {
            float normal_valid = 0.0;
            float3 view_normal = sample_view_normal(
                uv, raw_depth, normal_valid);
            debug_color = (raw_depth <= 0.0000001 || normal_valid < 0.5)
                ? 0.0.xxx
                : view_normal * 0.5 + 0.5;
        }
        else if (DebugMode > kDebugConfidence - 0.5)
        {
            debug_color = resolved.a.xxx;
        }
        debug_color = saturate(debug_color);
        if (OutputNeedsSrgbEncode > 0.5)
        {
            debug_color = linear_to_srgb(debug_color);
        }
        return float4(debug_color, 1.0);
    }

    return resolved;
}
