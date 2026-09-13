Texture2D<float4> InputTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer EasuBuffer : register(b0)
{
    float4 Con0;
    float2 InputSize;
    float2 OutputSize;
};

float approximate_reciprocal(float value)
{
    return asfloat(uint(0x7ef07ebb) - asuint(value));
}

float approximate_reciprocal_square_root(float value)
{
    return asfloat(uint(0x5f347d74) - (asuint(value) >> uint(1)));
}

float3 load_tap(int2 position)
{
    int2 clamped = clamp(position, int2(0, 0), int2(InputSize) - 1);
    return InputTexture.Load(int3(clamped, 0)).rgb;
}

float tap_luma(float3 color)
{
    return color.b * float(0.5) + (color.r * float(0.5) + color.g);
}

void easu_tap(
    inout float3 accumulated_color,
    inout float accumulated_weight,
    float2 pixel_offset,
    float2 gradient_direction,
    float2 anisotropy,
    float negative_lobe_strength,
    float clipping_point,
    float3 color)
{
    float2 rotated_offset;
    rotated_offset.x = (pixel_offset.x * (gradient_direction.x)) +
                       (pixel_offset.y * gradient_direction.y);
    rotated_offset.y = (pixel_offset.x * (-gradient_direction.y)) +
                       (pixel_offset.y * gradient_direction.x);
    rotated_offset *= anisotropy;
    float distance_squared = rotated_offset.x * rotated_offset.x +
                             rotated_offset.y * rotated_offset.y;
    distance_squared = min(distance_squared, clipping_point);

    float weight_b = float(2.0 / 5.0) * distance_squared + float(-1.0);
    float weight_a = negative_lobe_strength * distance_squared + float(-1.0);
    weight_b *= weight_b;
    weight_a *= weight_a;
    weight_b = float(25.0 / 16.0) * weight_b + float(-(25.0 / 16.0 - 1.0));
    float weight = weight_b * weight_a;

    accumulated_color += color * weight;
    accumulated_weight += weight;
}

void easu_set(
    inout float2 direction,
    inout float edge,
    float2 pp,
    bool bi_s,
    bool bi_t,
    bool bi_u,
    bool bi_v,
    float la,
    float lb,
    float lc,
    float ld,
    float le)
{
    float weight = float(0.0);
    if (bi_s)
        weight = (float(1.0) - pp.x) * (float(1.0) - pp.y);
    if (bi_t)
        weight = pp.x * (float(1.0) - pp.y);
    if (bi_u)
        weight = (float(1.0) - pp.x) * pp.y;
    if (bi_v)
        weight = pp.x * pp.y;

    float dc = ld - lc;
    float cb = lc - lb;
    float edge_x = max(abs(dc), abs(cb));
    edge_x = approximate_reciprocal(edge_x);
    float direction_x = ld - lb;
    direction.x += direction_x * weight;
    edge_x = saturate(abs(direction_x) * edge_x);
    edge_x *= edge_x;
    edge += edge_x * weight;

    float ec = le - lc;
    float ca = lc - la;
    float edge_y = max(abs(ec), abs(ca));
    edge_y = approximate_reciprocal(edge_y);
    float direction_y = le - la;
    direction.y += direction_y * weight;
    edge_y = saturate(abs(direction_y) * edge_y);
    edge_y *= edge_y;
    edge += edge_y * weight;
}

[numthreads(8, 8, 1)]
void CSEasu(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= uint(OutputSize.x) || id.y >= uint(OutputSize.y))
    {
        return;
    }

    float2 pp = float2(id.xy) * Con0.xy + Con0.zw;
    float2 fp = floor(pp);
    pp -= fp;
    int2 origin = int2(fp);

    float3 b = load_tap(origin + int2(0, -1));
    float3 c = load_tap(origin + int2(1, -1));
    float3 e = load_tap(origin + int2(-1, 0));
    float3 f = load_tap(origin + int2(0, 0));
    float3 g = load_tap(origin + int2(1, 0));
    float3 h = load_tap(origin + int2(2, 0));
    float3 i = load_tap(origin + int2(-1, 1));
    float3 j = load_tap(origin + int2(0, 1));
    float3 k = load_tap(origin + int2(1, 1));
    float3 l = load_tap(origin + int2(2, 1));
    float3 n = load_tap(origin + int2(0, 2));
    float3 o = load_tap(origin + int2(1, 2));

    float bl = tap_luma(b);
    float cl = tap_luma(c);
    float il = tap_luma(i);
    float jl = tap_luma(j);
    float fl = tap_luma(f);
    float el = tap_luma(e);
    float kl = tap_luma(k);
    float ll = tap_luma(l);
    float hl = tap_luma(h);
    float gl = tap_luma(g);
    float ol = tap_luma(o);
    float nl = tap_luma(n);

    float2 dir = float2(0.0, 0.0);
    float len = float(0.0);
    easu_set(dir, len, pp, true, false, false, false, bl, el, fl, gl, jl);
    easu_set(dir, len, pp, false, true, false, false, cl, fl, gl, hl, kl);
    easu_set(dir, len, pp, false, false, true, false, fl, il, jl, kl, nl);
    easu_set(dir, len, pp, false, false, false, true, gl, jl, kl, ll, ol);

    float2 dir2 = dir * dir;
    float dir_r = dir2.x + dir2.y;
    bool zro = dir_r < float(1.0 / 32768.0);
    dir_r = approximate_reciprocal_square_root(dir_r);
    dir_r = zro ? float(1.0) : dir_r;
    dir.x = zro ? float(1.0) : dir.x;
    dir *= float2(dir_r, dir_r);

    len = len * float(0.5);
    len *= len;

    float stretch = (dir.x * dir.x + dir.y * dir.y) *
                    approximate_reciprocal(max(abs(dir.x), abs(dir.y)));

    float2 len2 = float2(
        float(1.0) + (stretch - float(1.0)) * len,
        float(1.0) + float(-0.5) * len);

    float lob = float(0.5) + float((1.0 / 4.0 - 0.04) - 0.5) * len;

    float clp = approximate_reciprocal(lob);

    float3 min4 = min(min(f, min(g, j)), k);
    float3 max4 = max(max(f, max(g, j)), k);

    float3 ac = float3(0.0, 0.0, 0.0);
    float aw = float(0.0);
    easu_tap(ac, aw, float2(0.0, -1.0) - pp, dir, len2, lob, clp, b);
    easu_tap(ac, aw, float2(1.0, -1.0) - pp, dir, len2, lob, clp, c);
    easu_tap(ac, aw, float2(-1.0, 1.0) - pp, dir, len2, lob, clp, i);
    easu_tap(ac, aw, float2(0.0, 1.0) - pp, dir, len2, lob, clp, j);
    easu_tap(ac, aw, float2(0.0, 0.0) - pp, dir, len2, lob, clp, f);
    easu_tap(ac, aw, float2(-1.0, 0.0) - pp, dir, len2, lob, clp, e);
    easu_tap(ac, aw, float2(1.0, 1.0) - pp, dir, len2, lob, clp, k);
    easu_tap(ac, aw, float2(2.0, 1.0) - pp, dir, len2, lob, clp, l);
    easu_tap(ac, aw, float2(2.0, 0.0) - pp, dir, len2, lob, clp, h);
    easu_tap(ac, aw, float2(1.0, 0.0) - pp, dir, len2, lob, clp, g);
    easu_tap(ac, aw, float2(1.0, 2.0) - pp, dir, len2, lob, clp, o);
    easu_tap(ac, aw, float2(0.0, 2.0) - pp, dir, len2, lob, clp, n);

    float inverse_weight = float(1.0) / aw;
    float3 pix = min(
        max4,
        max(min4, ac * float3(inverse_weight, inverse_weight, inverse_weight)));
    OutputTexture[id.xy] = float4(pix, 1.0);
}
