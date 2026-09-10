Texture2D SourceTexture : register(t0);
SamplerState SourceSampler : register(s0);

cbuffer BloomBuffer : register(b0)
{
    float2 SourceTexelSize;
    float2 FilterRadius;

    float Threshold;
    float Knee;
    float InputNeedsSrgbDecode;
    float OutputNeedsSrgbEncode;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float3 decode(float3 color)
{
    if (InputNeedsSrgbDecode > 0.5)
    {
        float3 low = color / 12.92;
        float3 high = pow(max((color + 0.055) / 1.055, 0.0), 2.4);
        return lerp(high, low, step(color, 0.04045.xxx));
    }
    return color;
}

float3 sample_source(float2 uv)
{
    return decode(SourceTexture.Sample(SourceSampler, uv).rgb);
}

float3 box_downsample(float2 uv)
{
    float2 offset = SourceTexelSize * 0.5;
    return 0.25 * (
        sample_source(uv + float2(-offset.x, -offset.y)) +
        sample_source(uv + float2( offset.x, -offset.y)) +
        sample_source(uv + float2(-offset.x,  offset.y)) +
        sample_source(uv + float2( offset.x,  offset.y)));
}

float srgb_to_linear_scalar(float value)
{
    if (value <= 0.04045)
    {
        return value / 12.92;
    }
    return pow(max((value + 0.055) / 1.055, 0.0), 2.4);
}

float3 apply_threshold(float3 color)
{
    float threshold = srgb_to_linear_scalar(Threshold);
    float knee = max(srgb_to_linear_scalar(Threshold + Knee) - threshold,
                     0.0001);

    float brightness = max(color.r, max(color.g, color.b));
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);

    float contribution =
        max(soft, brightness - threshold) / max(brightness, 0.0001);
    return color * max(contribution, 0.0);
}

float4 PSBloomBright(VertexOutput input) : SV_Target
{
    return float4(apply_threshold(box_downsample(input.uv)), 1.0);
}

float4 PSBloomDownsample(VertexOutput input) : SV_Target
{
    return float4(box_downsample(input.uv), 1.0);
}

float4 PSBloomUpsample(VertexOutput input) : SV_Target
{
    float2 offset = SourceTexelSize * FilterRadius;
    float3 total =
        sample_source(input.uv + float2(-offset.x,  offset.y)) * 1.0 +
        sample_source(input.uv + float2(       0.0,  offset.y)) * 2.0 +
        sample_source(input.uv + float2( offset.x,  offset.y)) * 1.0 +
        sample_source(input.uv + float2(-offset.x,       0.0)) * 2.0 +
        sample_source(input.uv                                ) * 4.0 +
        sample_source(input.uv + float2( offset.x,       0.0)) * 2.0 +
        sample_source(input.uv + float2(-offset.x, -offset.y)) * 1.0 +
        sample_source(input.uv + float2(       0.0, -offset.y)) * 2.0 +
        sample_source(input.uv + float2( offset.x, -offset.y)) * 1.0;
    float3 result = total * (1.0 / 16.0);

    if (OutputNeedsSrgbEncode > 0.5)
    {
        float3 low = result * 12.92;
        float3 high = 1.055 * pow(max(result, 0.0), 1.0 / 2.4) - 0.055;
        result = lerp(high, low, step(result, 0.0031308.xxx));
    }
    return float4(result, 1.0);
}
