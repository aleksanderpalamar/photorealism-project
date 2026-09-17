#include "depth_view_space.hlsli"

Texture2D<float> DepthTexture : register(t0);
SamplerState DepthSampler : register(s1);

cbuffer OcclusionBuffer : register(b0)
{
    float NearPlane;
    float Radius;
    float Intensity;
    float Bias;

    float FadeStart;
    float FadeEnd;
    float EdgeRejection;
    float SampleCount;

    float2 DepthTexelSize;
    float2 ProjectionScale;

    float InteriorNearStart;
    float InteriorNearEnd;
    float InteriorRadius;
    float InteriorIntensity;

    float InteriorBias;
    float InteriorEdgeRejection;
    float Curve;
    float OcclusionPadding;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

static const float2 SampleDirections[16] =
{
    float2(1.0, 0.0), float2(-1.0, 0.0), float2(0.0, 1.0), float2(0.0, -1.0),
    float2(0.70710678, 0.70710678), float2(-0.70710678, 0.70710678),
    float2(0.70710678, -0.70710678), float2(-0.70710678, -0.70710678),
    float2(0.70710678, 0.70710678), float2(-0.70710678, 0.70710678),
    float2(0.70710678, -0.70710678), float2(-0.70710678, -0.70710678),
    float2(1.0, 0.0), float2(-1.0, 0.0), float2(0.0, 1.0), float2(0.0, -1.0)
};

static const float SampleRadii[16] =
{
    0.45, 0.45, 0.45, 0.45, 1.00, 1.00, 1.00, 1.00,
    0.45, 0.45, 0.45, 0.45, 1.00, 1.00, 1.00, 1.00
};

struct OcclusionProfile
{
    float radius;
    float intensity;
    float bias;
    float edge_rejection;
};

float sample_raw_depth(float2 uv)
{
    return saturate(DepthTexture.SampleLevel(DepthSampler, saturate(uv), 0.0));
}

OcclusionProfile profile_at(float distance)
{
    float interior = 1.0 - smoothstep(
        min(InteriorNearStart, InteriorNearEnd - 0.001), InteriorNearEnd, distance);
    OcclusionProfile profile;
    profile.radius = lerp(Radius, InteriorRadius, interior);
    profile.intensity = lerp(Intensity, InteriorIntensity, interior);
    profile.bias = lerp(Bias, InteriorBias, interior);
    profile.edge_rejection = lerp(EdgeRejection, InteriorEdgeRejection, interior);
    return profile;
}

float sample_occlusion(float2 uv, float3 normal, float3 center_position,
    float center_distance, OcclusionProfile profile, uint index)
{
    float2 projected_radius = profile.radius * ProjectionScale /
        max(2.0 * center_distance, 0.000001);
    float2 sample_uv = uv + SampleDirections[index] * projected_radius * SampleRadii[index];
    float inside = all(sample_uv > 0.0) && all(sample_uv < 1.0) ? 1.0 : 0.0;
    float raw_sample = sample_raw_depth(sample_uv);
    float valid = inside * (raw_sample > 0.0000001 ? 1.0 : 0.0);

    float sample_distance = linearize_reversed_depth(raw_sample, NearPlane);
    float edge_limit = profile.radius * max(profile.edge_rejection, 1.05);
    float edge_weight = 1.0 - smoothstep(
        profile.radius, edge_limit, abs(sample_distance - center_distance));

    float3 difference = reconstruct_view_position(
        sample_uv, sample_distance, ProjectionScale) - center_position;
    float sample_length = length(difference);
    float in_range = sample_length > 0.00001 && sample_length < edge_limit ? 1.0 : 0.0;
    float3 direction = difference / max(sample_length, 0.00001);
    float horizon = saturate(
        (dot(normal, direction) - profile.bias) / max(1.0 - profile.bias, 0.0001));
    float range_weight = 1.0 - smoothstep(profile.radius * 0.20, edge_limit, sample_length);
    return valid * in_range * horizon * range_weight * saturate(edge_weight);
}

float visibility_at(float2 uv)
{
    float raw_center = sample_raw_depth(uv);
    float center_distance = linearize_reversed_depth(raw_center, NearPlane);
    float normal_valid = 0.0;
    float3 normal = reconstruct_view_normal(uv, DepthTexelSize, NearPlane,
        ProjectionScale, raw_center,
        sample_raw_depth(uv - float2(DepthTexelSize.x, 0.0)),
        sample_raw_depth(uv + float2(DepthTexelSize.x, 0.0)),
        sample_raw_depth(uv - float2(0.0, DepthTexelSize.y)),
        sample_raw_depth(uv + float2(0.0, DepthTexelSize.y)),
        normal_valid);
    bool skip = raw_center <= 0.0000001 || center_distance >= FadeEnd || normal_valid < 0.5;
    OcclusionProfile profile = profile_at(center_distance);
    float3 center_position = reconstruct_view_position(uv, center_distance, ProjectionScale);

    uint count = (uint)clamp(SampleCount, 1.0, 16.0);
    float occlusion = 0.0;
    [loop]
    for (uint index = 0; index < count; ++index)
    {
        occlusion += sample_occlusion(
            uv, normal, center_position, center_distance, profile, index);
    }

    float normalized = pow(saturate(occlusion * 2.5 / float(count)), Curve);
    float fade = 1.0 - smoothstep(min(FadeStart, FadeEnd - 0.001), FadeEnd, center_distance);
    float visibility = saturate(1.0 - profile.intensity * normalized * fade);
    return skip ? 1.0 : visibility;
}

float4 PSAmbientOcclusion(VertexOutput input) : SV_Target
{
    float visibility = visibility_at(input.uv);
    return float4(visibility, visibility, visibility, 1.0);
}
