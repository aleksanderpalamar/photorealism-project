struct OverlayVertex
{
    float2 position;
    float2 uv;
    float4 color;
    float2 local;
    float2 half_extent;
    float radius;
    float kind;
    float2 padding;
};

StructuredBuffer<OverlayVertex> OverlayVertices : register(t0);
Texture2D FontTexture : register(t0);
SamplerState FontSampler : register(s0);

cbuffer OverlayBuffer : register(b0)
{
    float OutputNeedsSrgbEncode;
    float3 OverlayPadding;
};

struct OverlayPixel
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float2 local : TEXCOORD1;
    float3 shape : TEXCOORD2;
    float kind : TEXCOORD3;
};

OverlayPixel VSOverlay(uint vertex_id : SV_VertexID)
{
    OverlayVertex source = OverlayVertices[vertex_id];

    OverlayPixel output;
    output.position = float4(source.position, 0.0, 1.0);
    output.uv = source.uv;
    output.color = source.color;
    output.local = source.local;
    output.shape = float3(source.half_extent, source.radius);
    output.kind = source.kind;
    return output;
}

float rounded_box_distance(float2 local, float2 half_extent, float radius)
{
    float limit = min(half_extent.x, half_extent.y);
    float clamped_radius = clamp(radius, 0.0, limit);
    float2 inner = half_extent - clamped_radius;
    float2 delta = abs(local) - inner;
    float2 outside = max(delta, 0.0.xx);
    float inside = min(max(delta.x, delta.y), 0.0);
    return length(outside) + inside - clamped_radius;
}

float3 srgb_to_linear(float3 color)
{
    float3 low = color / 12.92;
    float3 high = pow(max((color + 0.055) / 1.055, 0.0), 2.4);
    float3 use_low = step(color, 0.04045.xxx);
    return lerp(high, low, use_low);
}

float4 PSOverlay(OverlayPixel input) : SV_Target
{
    float distance = rounded_box_distance(input.local, input.shape.xy, input.shape.z);
    float coverage = saturate(0.5 - distance);
    float mask = FontTexture.Sample(FontSampler, input.uv).r;

    float alpha = input.color.a;
    alpha *= lerp(coverage, mask, input.kind);

    float3 color = lerp(srgb_to_linear(input.color.rgb), input.color.rgb, OutputNeedsSrgbEncode);
    return float4(color, alpha);
}
