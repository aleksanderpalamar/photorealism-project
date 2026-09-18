#include "depth_view_space.hlsli"

Texture2D<float4> SceneTexture : register(t0);
Texture2D<float> VisibilityTexture : register(t1);
Texture2D<float> DepthTexture : register(t2);
SamplerState SceneSampler : register(s0);
SamplerState PointSampler : register(s1);

cbuffer ComposeBuffer : register(b0)
{
    float InputNeedsSrgbDecode;
    float OutputNeedsSrgbEncode;
    float DebugMode;
    float HalfResolution;

    float2 VisibilityTexelSize;
    float NearPlane;
    float RefinementEnabled;

    float HighlightStart;
    float HighlightEnd;
    float HighlightAoFloor;
    float ComposePadding;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float linear_distance(float2 uv)
{
    float raw_depth = saturate(DepthTexture.SampleLevel(PointSampler, uv, 0.0));
    return linearize_reversed_depth(raw_depth, NearPlane);
}

float bilateral_visibility(float2 uv)
{
    float2 position = uv / VisibilityTexelSize - 0.5;
    float2 base = floor(position);
    float2 fraction = position - base;
    float center = linear_distance(uv);

    float total = 0.0;
    float weights = 0.0;
    for (uint corner = 0; corner < 4; ++corner)
    {
        float2 offset = float2(corner & 1, (corner >> 1) & 1);
        float2 tap_uv = (base + offset + 0.5) * VisibilityTexelSize;
        float2 bilinear = lerp(1.0 - fraction, fraction, offset);
        float similarity = exp(-abs(linear_distance(tap_uv) - center) / max(0.05 * center, 0.01));
        float weight = bilinear.x * bilinear.y * similarity;
        total += VisibilityTexture.SampleLevel(PointSampler, tap_uv, 0.0) * weight;
        weights += weight;
    }
    if (weights <= 0.00001)
    {
        return 1.0;
    }
    return total / weights;
}

float4 PSComposeOcclusion(VertexOutput input) : SV_Target
{
    float3 scene = SceneTexture.SampleLevel(SceneSampler, input.uv, 0.0).rgb;
    scene = InputNeedsSrgbDecode > 0.5 ? srgb_to_linear(scene) : scene;

    float direct = VisibilityTexture.SampleLevel(PointSampler, input.uv, 0.0);
    float visibility = HalfResolution > 0.5 ? bilateral_visibility(input.uv) : direct;

    float luminance = dot(scene, float3(0.2126, 0.7152, 0.0722));
    float protection = smoothstep(
        min(HighlightStart, HighlightEnd - 0.0001), HighlightEnd, luminance);
    float weight = RefinementEnabled > 0.5
        ? lerp(1.0, saturate(HighlightAoFloor), protection)
        : 1.0;

    float3 color = scene * lerp(1.0, visibility, weight);
    color = DebugMode > 0.5 ? visibility.xxx : color;
    color = saturate(color);
    return float4(OutputNeedsSrgbEncode > 0.5 ? linear_to_srgb(color) : color, 1.0);
}
