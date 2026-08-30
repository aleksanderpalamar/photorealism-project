#include "depth_view_space.hlsli"

Texture2D<float4> SceneTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);
SamplerState SceneSampler : register(s0);
SamplerState DepthSampler : register(s1);

cbuffer SSAOBuffer : register(b0)
{
    float InputNeedsSrgbDecode;
    float OutputNeedsSrgbEncode;
    float NearPlane;
    float Radius;
    float Intensity;
    float Bias;
    float FadeStart;
    float FadeEnd;
    float EdgeRejection;
    float DebugMode;
    float2 DepthTexelSize;
    float2 ProjectionScale;
    float2 Padding;
    float RefinementEnabled;
    float HighlightStart;
    float HighlightEnd;
    float HighlightAoFloor;
    float InteriorEnabled;
    float InteriorNearStart;
    float InteriorNearEnd;
    float InteriorRadius;
    float InteriorIntensity;
    float InteriorBias;
    float InteriorEdgeRejection;
    float InteriorPadding;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

static const float2 SampleDirections[16] =
{
    float2(1.0, 0.0),
    float2(-1.0, 0.0),
    float2(0.0, 1.0),
    float2(0.0, -1.0),
    float2(0.70710678, 0.70710678),
    float2(-0.70710678, 0.70710678),
    float2(0.70710678, -0.70710678),
    float2(-0.70710678, -0.70710678),
    float2(0.70710678, 0.70710678),
    float2(-0.70710678, 0.70710678),
    float2(0.70710678, -0.70710678),
    float2(-0.70710678, -0.70710678),
    float2(1.0, 0.0),
    float2(-1.0, 0.0),
    float2(0.0, 1.0),
    float2(0.0, -1.0)
};

static const float SampleRadii[16] =
{
    0.45, 0.45, 0.45, 0.45,
    1.00, 1.00, 1.00, 1.00,
    0.45, 0.45, 0.45, 0.45,
    1.00, 1.00, 1.00, 1.00
};

float4 sample_scene(float2 uv)
{
    float4 color = SceneTexture.SampleLevel(SceneSampler, saturate(uv), 0.0);
    if (InputNeedsSrgbDecode > 0.5)
    {
        color.rgb = srgb_to_linear(color.rgb);
    }
    return color;
}

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

float calculate_visibility(float2 uv, float raw_center)
{
    if (raw_center <= 0.0000001)
    {
        return 1.0;
    }

    float center_distance = linearize_reversed_depth(raw_center, NearPlane);
    if (center_distance >= FadeEnd)
    {
        return 1.0;
    }

    // O ETS2 nao expõe uma etiqueta semantica para cabine. O perfil de
    // interior e, portanto, aplicado somente ao campo proximo: onde painel,
    // colunas, retrovisores e outros elementos da cabine dominam a imagem.
    // Em distancias maiores o perfil exterior aprovado da 0.8.0 permanece
    // integralmente ativo.
    float interior_weight = 0.0;
    if (InteriorEnabled > 0.5)
    {
        interior_weight = 1.0 - smoothstep(
            min(InteriorNearStart, InteriorNearEnd - 0.001),
            InteriorNearEnd,
            center_distance);
    }
    float active_radius = lerp(Radius, InteriorRadius, interior_weight);
    float active_intensity = lerp(
        Intensity, InteriorIntensity, interior_weight);
    float active_bias = lerp(Bias, InteriorBias, interior_weight);
    float active_edge_rejection = lerp(
        EdgeRejection,
        InteriorEdgeRejection,
        interior_weight);

    float normal_valid = 0.0;
    float3 normal = sample_view_normal(uv, raw_center, normal_valid);
    if (normal_valid < 0.5)
    {
        return 1.0;
    }

    float3 center_position = reconstruct_view_position(
        uv, center_distance, ProjectionScale);
    float2 projected_radius = float2(
        active_radius * ProjectionScale.x /
            max(2.0 * center_distance, 0.000001),
        active_radius * ProjectionScale.y /
            max(2.0 * center_distance, 0.000001));

    float occlusion = 0.0;
    int active_sample_count = RefinementEnabled > 0.5 ? 16 : 8;
    [unroll]
    for (int index = 0; index < 16; ++index)
    {
        if (index >= active_sample_count)
        {
            continue;
        }
        float2 sample_uv = uv +
            SampleDirections[index] * projected_radius * SampleRadii[index];
        if (sample_uv.x <= 0.0 || sample_uv.x >= 1.0 ||
            sample_uv.y <= 0.0 || sample_uv.y >= 1.0)
        {
            continue;
        }

        float raw_sample = sample_raw_depth(sample_uv);
        if (raw_sample <= 0.0000001)
        {
            continue;
        }

        float sample_distance = linearize_reversed_depth(raw_sample, NearPlane);
        float depth_jump = abs(sample_distance - center_distance);
        float edge_limit = active_radius * max(active_edge_rejection, 1.05);
        float edge_weight = 1.0 - smoothstep(
            active_radius,
            edge_limit,
            depth_jump);
        if (edge_weight <= 0.0)
        {
            continue;
        }

        float3 sample_position = reconstruct_view_position(
            sample_uv, sample_distance, ProjectionScale);
        float3 difference = sample_position - center_position;
        float sample_length = length(difference);
        if (sample_length <= 0.00001 || sample_length >= edge_limit)
        {
            continue;
        }

        float3 direction = difference / sample_length;
        float horizon = saturate(
            (dot(normal, direction) - active_bias) /
                max(1.0 - active_bias, 0.0001));
        float range_weight = 1.0 - smoothstep(
            active_radius * 0.20,
            edge_limit,
            sample_length);
        occlusion += horizon * range_weight * edge_weight;
    }

    float normalization = RefinementEnabled > 0.5 ? 0.15625 : 0.3125;
    float normalized_occlusion = saturate(occlusion * normalization);
    float distance_fade = 1.0 - smoothstep(
        min(FadeStart, FadeEnd - 0.001), FadeEnd, center_distance);
    return saturate(
        1.0 - active_intensity * normalized_occlusion * distance_fade);
}

float4 PSSSAO(VertexOutput input) : SV_Target
{
    float4 scene = sample_scene(input.uv);
    float raw_depth = sample_raw_depth(input.uv);
    float visibility = calculate_visibility(input.uv, raw_depth);

    float ao_weight = 1.0;
    if (RefinementEnabled > 0.5)
    {
        float luminance = dot(scene.rgb, float3(0.2126, 0.7152, 0.0722));
        float highlight_protection = smoothstep(
            min(HighlightStart, HighlightEnd - 0.0001),
            HighlightEnd,
            luminance);
        ao_weight = lerp(
            1.0,
            saturate(HighlightAoFloor),
            highlight_protection);
    }

    float applied_visibility = lerp(1.0, visibility, ao_weight);
    float3 color = scene.rgb * applied_visibility;
    if (DebugMode > 0.5)
    {
        color = visibility.xxx;
    }

    color = saturate(color);
    if (OutputNeedsSrgbEncode > 0.5)
    {
        color = linear_to_srgb(color);
    }
    return float4(saturate(color), scene.a);
}
