#include "depth_view_space.hlsli"

Texture2D<float4> SceneTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);
SamplerState SceneSampler : register(s0);
SamplerState DepthSampler : register(s1);

cbuffer InteriorLightBuffer : register(b0)
{
    float InputNeedsSrgbDecode;
    float OutputNeedsSrgbEncode;
    float NearPlane;
    float Strength;

    float NearStart;
    float NearEnd;
    float ExteriorLuma;
    float Gain;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float cabin_weight(float2 uv)
{
    float raw_depth = saturate(DepthTexture.SampleLevel(DepthSampler, uv, 0.0));
    float distance = linearize_reversed_depth(raw_depth, NearPlane);
    float sky = raw_depth <= 0.0000001 ? 0.0 : 1.0;
    return sky * (1.0 - smoothstep(NearStart, NearEnd, distance));
}

float4 PSInteriorLight(VertexOutput input) : SV_Target
{
    float3 color = SceneTexture.SampleLevel(SceneSampler, input.uv, 0.0).rgb;
    color = InputNeedsSrgbDecode > 0.5 ? srgb_to_linear(color) : color;

    float fill = 1.0 + Strength * Gain * saturate(ExteriorLuma);
    color *= lerp(1.0, fill, cabin_weight(input.uv));

    color = saturate(color);
    return float4(OutputNeedsSrgbEncode > 0.5 ? linear_to_srgb(color) : color, 1.0);
}
