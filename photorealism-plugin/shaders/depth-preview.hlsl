#include "depth_view_space.hlsli"

Texture2D<float> DepthTexture : register(t0);
SamplerState DepthSampler : register(s0);

cbuffer DepthPreviewBuffer : register(b0)
{
    float PreviewMode;
    float OutputNeedsSrgbEncode;
    float NearPlane;
    float PreviewDistance;
    float2 TexelSize;
    float2 ProjectionScale;
};

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
        TexelSize,
        NearPlane,
        ProjectionScale,
        raw_center,
        sample_raw_depth(uv - float2(TexelSize.x, 0.0)),
        sample_raw_depth(uv + float2(TexelSize.x, 0.0)),
        sample_raw_depth(uv - float2(0.0, TexelSize.y)),
        sample_raw_depth(uv + float2(0.0, TexelSize.y)),
        normal_valid);
}

float4 PSDepthPreview(VertexOutput input) : SV_Target
{
    float raw_depth = sample_raw_depth(input.uv);
    float visible_depth = raw_depth;
    float3 color = visible_depth.xxx;

    if (PreviewMode > 3.5)
    {
        if (raw_depth <= 0.0000001)
        {
            color = 0.0.xxx;
        }
        else
        {
            float normal_valid = 0.0;
            float3 view_normal = sample_view_normal(
                input.uv, raw_depth, normal_valid);
            color = normal_valid < 0.5
                ? 0.0.xxx
                : view_normal * 0.5 + 0.5;
        }
    }
    else if (PreviewMode > 2.5)
    {
        float linear_distance = linearize_reversed_depth(
            raw_depth, NearPlane);
        visible_depth = saturate(
            linear_distance / max(PreviewDistance, 1.0));
        color = visible_depth.xxx;
    }
    else if (PreviewMode > 1.5)
    {
        visible_depth = saturate(
            -log2(max(raw_depth, 0.0000001)) / 16.0);
        color = visible_depth.xxx;
    }

    if (OutputNeedsSrgbEncode > 0.5)
    {
        color = linear_to_srgb(color);
    }
    return float4(saturate(color), 1.0);
}
