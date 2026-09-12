Texture2D<float4> UpscaledTexture : register(t0);

cbuffer RcasBuffer : register(b0)
{
    float2 OutputSize;
    float Attenuation;
    float RcasPadding;
};

struct RcasPixel
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

RcasPixel VSRcas(uint vertex_id : SV_VertexID)
{
    RcasPixel output;
    output.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position =
        float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float3 load_upscaled(int2 position)
{
    int2 clamped = clamp(position, int2(0, 0), int2(OutputSize) - 1);
    return UpscaledTexture.Load(int3(clamped, 0)).rgb;
}

float ring_luma(float3 color)
{
    return color.b * 0.5 + (color.r * 0.5 + color.g);
}

float4 PSRcas(RcasPixel input) : SV_Target
{
    int2 position = int2(input.uv * OutputSize);

    float3 up = load_upscaled(position + int2(0, -1));
    float3 left = load_upscaled(position + int2(-1, 0));
    float3 center = load_upscaled(position);
    float3 right = load_upscaled(position + int2(1, 0));
    float3 down = load_upscaled(position + int2(0, 1));

    float3 ring_min = min(min(up, left), min(right, down));
    float3 ring_max = max(max(up, left), max(right, down));

    float3 hit_min = ring_min / max(4.0 * ring_max, 1.0 / 32768.0);
    float3 hit_max =
        (1.0 - ring_max) / min(4.0 * ring_min - 4.0, -1.0 / 32768.0);
    float3 lobe_rgb = max(-hit_min, hit_max);
    float lobe = max(-0.1875, min(max(max(lobe_rgb.r, lobe_rgb.g), lobe_rgb.b), 0.0));
    lobe *= Attenuation;

    float up_luma = ring_luma(up);
    float left_luma = ring_luma(left);
    float center_luma = ring_luma(center);
    float right_luma = ring_luma(right);
    float down_luma = ring_luma(down);

    float noise = 0.25 * (up_luma + left_luma + right_luma + down_luma) -
                  center_luma;
    float span = max(max(max(up_luma, left_luma), max(right_luma, down_luma)),
                     center_luma) -
                 min(min(min(up_luma, left_luma), min(right_luma, down_luma)),
                     center_luma);
    noise = saturate(abs(noise) / max(span, 1.0 / 32768.0));
    lobe *= -0.5 * noise + 1.0;

    float3 sharpened = (lobe * (up + left + right + down) + center) /
                       (4.0 * lobe + 1.0);
    return float4(clamp(sharpened, 0.0, 1.0), 1.0);
}
