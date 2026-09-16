cbuffer photorealism_surface_constants : register(b13)
{
    float4 photorealism_rain;
    float4 photorealism_road;
    float4 photorealism_frame;
    float4 photorealism_mask;
};

static const float kPhotorealismPi = 3.14159265;

float photorealism_luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

bool photorealism_is_road(uint4 material)
{
    return material.y == (uint)photorealism_mask.x;
}

float3 photorealism_view_position(float2 pixel, float view_depth)
{
    float2 ndc = pixel * photorealism_frame.zw * 2.0 - 1.0;
    ndc.y = -ndc.y;
    float2 plane = ndc / max(photorealism_frame.xy, float2(1e-4, 1e-4));
    return float3(plane * view_depth, view_depth);
}

float3 photorealism_detail_normal(
    Texture2D<float4> albedo, SamplerState state, float2 uv, float strength)
{
    uint width = 0;
    uint height = 0;
    uint levels = 0;
    albedo.GetDimensions(0, width, height, levels);
    float2 texel = 1.0 / max(float2(width, height), float2(1.0, 1.0));

    float h00 = photorealism_luminance(albedo.SampleLevel(state, uv + float2(-texel.x, -texel.y), 0).rgb);
    float h10 = photorealism_luminance(albedo.SampleLevel(state, uv + float2(0.0, -texel.y), 0).rgb);
    float h20 = photorealism_luminance(albedo.SampleLevel(state, uv + float2(texel.x, -texel.y), 0).rgb);
    float h01 = photorealism_luminance(albedo.SampleLevel(state, uv + float2(-texel.x, 0.0), 0).rgb);
    float h21 = photorealism_luminance(albedo.SampleLevel(state, uv + float2(texel.x, 0.0), 0).rgb);
    float h02 = photorealism_luminance(albedo.SampleLevel(state, uv + float2(-texel.x, texel.y), 0).rgb);
    float h12 = photorealism_luminance(albedo.SampleLevel(state, uv + float2(0.0, texel.y), 0).rgb);
    float h22 = photorealism_luminance(albedo.SampleLevel(state, uv + float2(texel.x, texel.y), 0).rgb);

    float gx = (h20 + 2.0 * h21 + h22) - (h00 + 2.0 * h01 + h02);
    float gy = (h02 + 2.0 * h12 + h22) - (h00 + 2.0 * h10 + h20);

    gx = clamp(gx * strength, -0.5, 0.5);
    gy = clamp(gy * strength, -0.5, 0.5);
    return normalize(float3(gx, gy, 1.0));
}

float3 photorealism_ripple_normal(float2 plane, float wetness, float time)
{
    if (wetness <= 0.002)
    {
        return float3(0.0, 0.0, 1.0);
    }

    float2 accumulated = float2(0.0, 0.0);
    [unroll]
    for (int layer = 0; layer < 2; ++layer)
    {
        float scale = layer == 0 ? 1.0 : 1.7;
        float2 scaled = plane * scale + float2(layer * 0.37, layer * 0.61);
        float2 cell = floor(scaled);
        float2 local = frac(scaled) - 0.5;

        float noise = frac(sin(dot(cell, float2(127.1, 311.7))) * 43758.5453);
        float life = frac(time * (0.9 + 0.25 * layer) + noise);
        float radius = length(local);
        float front = saturate(1.0 - abs(radius - life * 0.6) * 7.0);
        float decay = 1.0 - life;
        float wave = sin((radius * 14.0 - life * 11.0) * kPhotorealismPi);
        float2 direction = radius > 1e-4 ? local / radius : float2(0.0, 0.0);
        accumulated += direction * wave * front * decay;
    }

    return normalize(float3(accumulated * 0.4 * wetness, 1.0));
}

void photorealism_wet_surface(
    inout float4 o0,
    inout float4 o1,
    inout float4 o2,
    inout uint4 o3,
    float4 position,
    Texture2D<float4> albedo,
    SamplerState state,
    float2 uv,
    float reflection_scale)
{
    if (photorealism_rain.w < 0.5 || !photorealism_is_road(o3))
    {
        return;
    }

    float reflectivity = saturate(float(o3.z) / max(reflection_scale, 1.0));
    float wet_amount = saturate(photorealism_rain.x) * max(reflectivity, photorealism_mask.y);
    if (wet_amount <= 0.002)
    {
        return;
    }

    float view_depth = abs(o0.w);
    if (view_depth <= 0.01)
    {
        return;
    }

    float3 view_normal = normalize(o0.xyz);
    float3 view_position = photorealism_view_position(position.xy, view_depth);

    float near_field = 1.0 - saturate((view_depth - 25.0) / 45.0);
    float wetness = saturate(wet_amount * (0.35 + 0.65 * near_field));

    float3 detail = photorealism_detail_normal(albedo, state, uv, photorealism_road.x);
    float3 ripple = photorealism_ripple_normal(
        uv * photorealism_rain.y, wetness * near_field, photorealism_rain.z);

    float3 surface = detail;
    surface.xy *= lerp(photorealism_road.y, 1.0, wet_amount);
    surface.xy *= 1.0 - saturate(wetness * 0.65);
    surface = normalize(surface);
    surface = normalize(float3(surface.xy + ripple.xy, surface.z * ripple.z));

    float3 dx = ddx(view_position);
    float3 dy = ddy(view_position);
    float2 du = ddx(uv);
    float2 dv = ddy(uv);

    float3 tangent = dx * dv.y - dy * du.y;
    float3 bitangent = dy * du.x - dx * dv.x;
    if (dot(tangent, tangent) < 1e-10 || dot(bitangent, bitangent) < 1e-10)
    {
        return;
    }
    tangent = normalize(tangent);
    tangent = normalize(tangent - view_normal * dot(view_normal, tangent));
    float3 corrected = cross(view_normal, tangent);
    bitangent = corrected * sign(dot(normalize(bitangent), corrected));

    float3x3 frame = float3x3(tangent, bitangent, view_normal);
    o0.xyz = normalize(mul(surface, frame));

    o2.rgb *= lerp(1.0, photorealism_road.w, wetness);
    o1.rgb = lerp(o1.rgb, float3(1.0, 1.0, 1.0), wetness * photorealism_road.z);
    o1.a = lerp(o1.a, photorealism_road.z > 0.0 ? 900.0 : o1.a, wetness);
}
