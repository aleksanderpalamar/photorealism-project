#include "depth_view_space.hlsli"

Texture2D<float4> CurrentTexture : register(t0);
Texture2D<float4> HistoryTexture : register(t1);
Texture2D<float> CurrentDepthTexture : register(t2);
Texture2D<float> HistoryDepthTexture : register(t3);

SamplerState ColorSampler : register(s0);
SamplerState DepthSampler : register(s1);

cbuffer TemporalBuffer : register(b0)
{
    float2 TexelSize;
    float NearPlane;
    float HistoryWeight;

    float DepthRejection;
    float ColorRejection;
    float HistoryValid;
    float OutputNeedsSrgbEncode;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float depth_history_confidence(float current_raw, float history_raw)
{
    bool current_sky = current_raw <= 0.0000001;
    bool history_sky = history_raw <= 0.0000001;
    if (current_sky || history_sky)
    {
        return current_sky && history_sky ? 1.0 : 0.0;
    }

    float current_distance = linearize_reversed_depth(
        current_raw, NearPlane);
    float history_distance = linearize_reversed_depth(
        history_raw, NearPlane);
    float relative_difference =
        abs(current_distance - history_distance) /
        max(min(current_distance, history_distance), 0.1);
    return 1.0 - smoothstep(
        DepthRejection,
        max(DepthRejection * 2.0, DepthRejection + 0.0001),
        relative_difference);
}

float4 PSTemporal(VertexOutput input) : SV_Target
{
    float2 uv = saturate(input.uv);
    float4 current = CurrentTexture.SampleLevel(ColorSampler, uv, 0.0);
    float3 resolved = current.rgb;

    if (HistoryValid > 0.5)
    {
        float3 minimum_color = current.rgb;
        float3 maximum_color = current.rgb;

        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            [unroll]
            for (int x = -1; x <= 1; ++x)
            {
                float2 offset = float2(x, y) * TexelSize;
                float3 neighbour = CurrentTexture.SampleLevel(
                    ColorSampler, saturate(uv + offset), 0.0).rgb;
                minimum_color = min(minimum_color, neighbour);
                maximum_color = max(maximum_color, neighbour);
            }
        }

        float3 history = HistoryTexture.SampleLevel(
            ColorSampler, uv, 0.0).rgb;
        float3 clipped_history = clamp(
            history,
            minimum_color,
            maximum_color);

        float current_raw = CurrentDepthTexture.SampleLevel(
            DepthSampler, uv, 0.0);
        float history_raw = HistoryDepthTexture.SampleLevel(
            DepthSampler, uv, 0.0);
        float depth_confidence = depth_history_confidence(
            current_raw, history_raw);

        float color_difference = length(current.rgb - clipped_history);
        float color_confidence = 1.0 - smoothstep(
            ColorRejection,
            max(ColorRejection * 2.0, ColorRejection + 0.0001),
            color_difference);

        float accepted_history = saturate(HistoryWeight) *
            depth_confidence * color_confidence;
        resolved = lerp(current.rgb, clipped_history, accepted_history);
    }

    resolved = saturate(resolved);
    if (OutputNeedsSrgbEncode > 0.5)
    {
        resolved = linear_to_srgb(resolved);
    }
    return float4(saturate(resolved), current.a);
}
