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

float3 linear_to_srgb(float3 color)
{
    color = max(color, 0.0);
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(color, 1.0 / 2.4) - 0.055;
    float3 use_low = step(color, 0.0031308.xxx);
    return lerp(high, low, use_low);
}

float sample_raw_depth(float2 uv)
{
    return saturate(DepthTexture.SampleLevel(
        DepthSampler, saturate(uv), 0.0));
}

float linearize_reversed_depth(float raw_depth)
{
    return max(NearPlane, 0.000001) / max(raw_depth, 0.0000001);
}

float3 reconstruct_view_position(float2 uv, float linear_distance)
{
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;
    return float3(
        ndc.x * linear_distance / max(ProjectionScale.x, 0.000001),
        ndc.y * linear_distance / max(ProjectionScale.y, 0.000001),
        linear_distance);
}

float3 reconstruct_view_normal(float2 uv, float raw_center)
{
    float center_distance = linearize_reversed_depth(raw_center);
    float raw_left = sample_raw_depth(uv - float2(TexelSize.x, 0.0));
    float raw_right = sample_raw_depth(uv + float2(TexelSize.x, 0.0));
    float raw_up = sample_raw_depth(uv - float2(0.0, TexelSize.y));
    float raw_down = sample_raw_depth(uv + float2(0.0, TexelSize.y));

    float left_distance = linearize_reversed_depth(raw_left);
    float right_distance = linearize_reversed_depth(raw_right);
    float up_distance = linearize_reversed_depth(raw_up);
    float down_distance = linearize_reversed_depth(raw_down);

    float3 center = reconstruct_view_position(uv, center_distance);
    float3 left = reconstruct_view_position(
        uv - float2(TexelSize.x, 0.0), left_distance);
    float3 right = reconstruct_view_position(
        uv + float2(TexelSize.x, 0.0), right_distance);
    float3 up = reconstruct_view_position(
        uv - float2(0.0, TexelSize.y), up_distance);
    float3 down = reconstruct_view_position(
        uv + float2(0.0, TexelSize.y), down_distance);

    float3 horizontal_forward = right - center;
    float3 horizontal_backward = center - left;
    float3 vertical_forward = down - center;
    float3 vertical_backward = center - up;

    float3 horizontal =
        abs(horizontal_forward.z) < abs(horizontal_backward.z)
            ? horizontal_forward
            : horizontal_backward;
    float3 vertical =
        abs(vertical_forward.z) < abs(vertical_backward.z)
            ? vertical_forward
            : vertical_backward;

    float3 normal = normalize(cross(horizontal, vertical));
    if (normal.z > 0.0)
    {
        normal = -normal;
    }
    return normal;
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
            float3 view_normal = reconstruct_view_normal(input.uv, raw_depth);
            color = view_normal * 0.5 + 0.5;
        }
    }
    else if (PreviewMode > 2.5)
    {
        float linear_distance = linearize_reversed_depth(raw_depth);
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
