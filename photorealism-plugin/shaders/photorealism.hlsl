Texture2D SceneTexture : register(t0);
SamplerState SceneSampler : register(s0);

cbuffer SettingsBuffer : register(b0)
{
    float2 TexelSize;
    float Exposure;
    float Temperature;

    float Contrast;
    float Saturation;
    float Vibrance;
    float Shadows;

    float Highlights;
    float Blacks;
    float Whites;
    float LocalContrast;

    float Sharpness;
    float Vignette;
    float InputNeedsSrgbDecode;
    float OutputNeedsSrgbEncode;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(uint vertex_id : SV_VertexID)
{
    VertexOutput output;
    output.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 srgb_to_linear(float3 color)
{
    float3 low = color / 12.92;
    float3 high = pow(max((color + 0.055) / 1.055, 0.0), 2.4);
    float3 use_low = step(color, 0.04045.xxx);
    return lerp(high, low, use_low);
}

float3 linear_to_srgb(float3 color)
{
    color = max(color, 0.0);
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(color, 1.0 / 2.4) - 0.055;
    float3 use_low = step(color, 0.0031308.xxx);
    return lerp(high, low, use_low);
}

float4 sample_scene(float2 uv)
{
    float4 sample_value = SceneTexture.Sample(SceneSampler, uv);
    if (InputNeedsSrgbDecode > 0.5)
    {
        sample_value.rgb = srgb_to_linear(sample_value.rgb);
    }
    return sample_value;
}

float3 apply_temperature(float3 color)
{
    float shift = clamp((Temperature - 6500.0) / 3500.0, -1.0, 1.0);
    float3 balance = float3(1.0 - 0.08 * shift, 1.0, 1.0 + 0.10 * shift);
    return color * balance;
}

float3 apply_tonal_controls(float3 color)
{
    color = max(color * exp2(Exposure), 0.0);
    color = apply_temperature(color);

    float initial_luma = luminance(color);
    float shadow_mask = 1.0 - smoothstep(0.025, 0.35, initial_luma);
    float highlight_mask = smoothstep(0.45, 1.0, initial_luma);
    color *= exp2(Shadows * shadow_mask + Highlights * highlight_mask);

    float black_mask = 1.0 - smoothstep(0.0, 0.16, initial_luma);
    float white_mask = smoothstep(0.55, 1.0, initial_luma);
    color += Blacks * black_mask * 0.06;
    color += Whites * white_mask * 0.06;

    const float pivot = 0.18;
    color = max((color - pivot) * Contrast + pivot, 0.0);

    float luma = luminance(color);
    color = lerp(luma.xxx, color, Saturation);

    float maximum = max(color.r, max(color.g, color.b));
    float minimum = min(color.r, min(color.g, color.b));
    float chroma = saturate(maximum - minimum);
    float vibrance_factor = 1.0 + Vibrance * (1.0 - chroma);
    color = lerp(luminance(color).xxx, color, vibrance_factor);
    return max(color, 0.0);
}

float4 PSMain(VertexOutput input) : SV_Target
{
    float4 source = sample_scene(input.uv);
    float3 center = source.rgb;

    float3 neighbours =
        sample_scene(input.uv + float2(TexelSize.x, 0.0)).rgb +
        sample_scene(input.uv - float2(TexelSize.x, 0.0)).rgb +
        sample_scene(input.uv + float2(0.0, TexelSize.y)).rgb +
        sample_scene(input.uv - float2(0.0, TexelSize.y)).rgb;
    float3 blur = neighbours * 0.25;
    float3 detail = center - blur;
    float edge = abs(luminance(center) - luminance(blur));
    float edge_mask = smoothstep(0.0015, 0.08, edge);
    float center_luma = luminance(center);
    float midtone_mask =
        smoothstep(0.03, 0.18, center_luma) *
        (1.0 - smoothstep(0.55, 0.90, center_luma));
    center += detail *
        (Sharpness + LocalContrast * edge_mask * midtone_mask);

    float3 color = apply_tonal_controls(center);

    float2 vignette_uv = input.uv * (1.0 - input.uv);
    float vignette_shape = saturate(vignette_uv.x * vignette_uv.y * 16.0);
    color *= lerp(1.0, smoothstep(0.0, 1.0, vignette_shape), Vignette);

    color = saturate(color);
    if (OutputNeedsSrgbEncode > 0.5)
    {
        color = linear_to_srgb(color);
    }

    return float4(saturate(color), source.a);
}
