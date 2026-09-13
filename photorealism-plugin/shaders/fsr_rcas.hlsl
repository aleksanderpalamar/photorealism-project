Texture2D<float4> UpscaledTexture : register(t0);
Texture2D<float> GrainTexture : register(t1);

cbuffer RcasBuffer : register(b0)
{
    float RcasCon;
    float DecodeBeforeWrite;
    float GrainAmount;
    float GrainPhase;
    float2 OutputSize;
    float GrainTileSize;
    float RcasPadding;
};

#define FSR_RCAS_LIMIT (0.25 - (1.0 / 16.0))

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

float approximate_reciprocal_medium(float value)
{
    float b = asfloat(uint(0x7ef19fff) - asuint(value));
    return b * (-b * value + float(2.0));
}

float4 rcas_load(int2 position)
{
    int2 clamped = clamp(position, int2(0, 0), int2(OutputSize) - 1);
    return UpscaledTexture.Load(int3(clamped, 0));
}

void rcas_input(inout float r, inout float g, inout float b)
{
}

float3 rcas_filter(uint2 ip)
{
    int2 sp = int2(ip);
    float3 b = rcas_load(sp + int2(0, -1)).rgb;
    float3 d = rcas_load(sp + int2(-1, 0)).rgb;
    float3 e = rcas_load(sp).rgb;
    float3 f = rcas_load(sp + int2(1, 0)).rgb;
    float3 h = rcas_load(sp + int2(0, 1)).rgb;

    float br = b.r;
    float bg = b.g;
    float bb = b.b;
    float dr = d.r;
    float dg = d.g;
    float db = d.b;
    float er = e.r;
    float eg = e.g;
    float eb = e.b;
    float fr = f.r;
    float fg = f.g;
    float fb = f.b;
    float hr = h.r;
    float hg = h.g;
    float hb = h.b;

    rcas_input(br, bg, bb);
    rcas_input(dr, dg, db);
    rcas_input(er, eg, eb);
    rcas_input(fr, fg, fb);
    rcas_input(hr, hg, hb);

    float mn4r = min(min(br, min(dr, fr)), hr);
    float mn4g = min(min(bg, min(dg, fg)), hg);
    float mn4b = min(min(bb, min(db, fb)), hb);
    float mx4r = max(max(br, max(dr, fr)), hr);
    float mx4g = max(max(bg, max(dg, fg)), hg);
    float mx4b = max(max(bb, max(db, fb)), hb);

    float2 peak_c = float2(1.0, -1.0 * 4.0);

    float hit_min_r = mn4r * rcp(float(4.0) * mx4r);
    float hit_min_g = mn4g * rcp(float(4.0) * mx4g);
    float hit_min_b = mn4b * rcp(float(4.0) * mx4b);
    float hit_max_r = (peak_c.x - mx4r) * rcp(float(4.0) * mn4r + peak_c.y);
    float hit_max_g = (peak_c.x - mx4g) * rcp(float(4.0) * mn4g + peak_c.y);
    float hit_max_b = (peak_c.x - mx4b) * rcp(float(4.0) * mn4b + peak_c.y);
    float lobe_r = max(-hit_min_r, hit_max_r);
    float lobe_g = max(-hit_min_g, hit_max_g);
    float lobe_b = max(-hit_min_b, hit_max_b);
    float lobe = max(
        float(-FSR_RCAS_LIMIT),
        min(max(lobe_r, max(lobe_g, lobe_b)), float(0.0))) * RcasCon;

    float rcp_l = approximate_reciprocal_medium(float(4.0) * lobe + float(1.0));
    float3 pix;
    pix.r = (lobe * br + lobe * dr + lobe * hr + lobe * fr + er) * rcp_l;
    pix.g = (lobe * bg + lobe * dg + lobe * hg + lobe * fg + eg) * rcp_l;
    pix.b = (lobe * bb + lobe * db + lobe * hb + lobe * fb + eb) * rcp_l;
    return pix;
}

void lfga(inout float3 c, float3 t, float a)
{
    c += (t * float3(a, a, a)) * min(float3(1.0, 1.0, 1.0) - c, c);
}

float3 srgb_to_linear(float3 color)
{
    float3 low = color / 12.92;
    float3 high = pow(max((color + 0.055) / 1.055, 0.0), 2.4);
    return lerp(high, low, step(color, 0.04045.xxx));
}

float3 linear_to_srgb(float3 color)
{
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(max(color, 0.0), 1.0 / 2.4) - 0.055;
    return lerp(high, low, step(color, 0.0031308.xxx));
}

float grain_at(uint2 ip)
{
    uint tile = uint(GrainTileSize);
    float uniform_value = GrainTexture.Load(int3(ip % tile, 0));
    return frac(uniform_value + GrainPhase) - 0.5;
}

float4 PSRcas(RcasPixel input) : SV_Target
{
    uint2 ip = uint2(input.position.xy);
    float3 linear_color = srgb_to_linear(saturate(rcas_filter(ip)));
    float grain = grain_at(ip);
    lfga(linear_color, float3(grain, grain, grain), GrainAmount);
    float3 encoded = linear_to_srgb(saturate(linear_color));
    float3 written = lerp(encoded, linear_color, DecodeBeforeWrite);
    return float4(written, 1.0);
}
