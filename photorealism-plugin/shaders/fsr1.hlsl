// AMD FidelityFX Super Resolution 1 integration shader.
// Based on GPUOpen FidelityFX-FSR v1.0.2, licensed under MIT.
// See third_party/fidelityfx-fsr/LICENSE.txt.

cbuffer FsrConstants : register(b0) {
    uint4 Const0;
    uint4 Const1;
    uint4 Const2;
    uint4 Const3;
    uint4 Config;
};

#define A_GPU 1
#define A_HLSL 1
#define FSR_EASU_F 1
#define FSR_RCAS_F 1

#include "../third_party/fidelityfx-fsr/ffx_a.h"

SamplerState LinearClamp : register(s0);
Texture2D InputTexture : register(t0);
Texture2D HistoryTexture : register(t1);
RWTexture2D<float4> OutputTexture : register(u0);

float3 AaWorkingColor(float3 color) {
    if (Config.x != 0u) {
        color *= rcp(max(color.r, max(color.g, color.b)) + 1.0);
    }
    return color;
}

float3 AaOutputColor(float3 color) {
    if (Config.x != 0u) {
        color *= rcp(max(1.0 / 32768.0,
            1.0 - max(color.r, max(color.g, color.b))));
    }
    return color;
}

float AaLuma(float3 color) {
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 SampleAaInput(float2 uv) {
    return AaWorkingColor(InputTexture.SampleLevel(LinearClamp, uv, 0.0).rgb);
}

float3 SpatialEdgeAa(float2 uv, float2 texel, out float edge_strength) {
    const float3 center = SampleAaInput(uv);
    const float3 north = SampleAaInput(uv + float2(0.0, -texel.y));
    const float3 south = SampleAaInput(uv + float2(0.0, texel.y));
    const float3 west = SampleAaInput(uv + float2(-texel.x, 0.0));
    const float3 east = SampleAaInput(uv + float2(texel.x, 0.0));
    const float lc = AaLuma(center);
    const float ln = AaLuma(north);
    const float ls = AaLuma(south);
    const float lw = AaLuma(west);
    const float le = AaLuma(east);
    const float gradient_x = abs(lw - le);
    const float gradient_y = abs(ln - ls);
    const float local_min = min(lc, min(min(ln, ls), min(lw, le)));
    const float local_max = max(lc, max(max(ln, ls), max(lw, le)));
    edge_strength = local_max - local_min;
    if (edge_strength < max(0.015, local_max * 0.04)) return center;
    const float3 tangent_average = gradient_y > gradient_x
        ? (west + east) * 0.5
        : (north + south) * 0.5;
    const float blend = saturate(edge_strength * 3.5) * 0.38;
    return lerp(center, tangent_average, blend);
}

[numthreads(8, 8, 1)]
void CSTemporalAa(uint3 dispatch_id : SV_DispatchThreadID) {
    const uint2 dimensions = Config.zw;
    if (dispatch_id.x >= dimensions.x || dispatch_id.y >= dimensions.y) return;
    const float2 texel = asfloat(Const0.xy);
    const float2 uv = (float2(dispatch_id.xy) + 0.5) * texel;
    float edge_strength = 0.0;
    const float3 current = SpatialEdgeAa(uv, texel, edge_strength);
    float3 resolved = current;
    if (Config.y != 0u) {
        float best_difference = 1e9;
        float3 best_history = current;
        int2 best_offset = int2(0, 0);
        [unroll]
        for (int y = -1; y <= 1; ++y) {
            [unroll]
            for (int x = -1; x <= 1; ++x) {
                float3 history = HistoryTexture.SampleLevel(
                    LinearClamp, uv + float2(x, y) * texel, 0.0).rgb;
                history = AaWorkingColor(history);
                const float difference =
                    abs(AaLuma(history) - AaLuma(current)) +
                    length(history - current) * 0.20;
                if (difference < best_difference) {
                    best_difference = difference;
                    best_history = history;
                    best_offset = int2(x, y);
                }
            }
        }
        float3 neighborhood_min = current;
        float3 neighborhood_max = current;
        [unroll]
        for (int y = -1; y <= 1; ++y) {
            [unroll]
            for (int x = -1; x <= 1; ++x) {
                const float3 sample_color =
                    SampleAaInput(uv + float2(x, y) * texel);
                neighborhood_min = min(neighborhood_min, sample_color);
                neighborhood_max = max(neighborhood_max, sample_color);
            }
        }
        best_history = clamp(best_history, neighborhood_min, neighborhood_max);
        const bool moved = any(best_offset != int2(0, 0));
        const float stable = saturate(1.0 - best_difference * 7.5);
        const float history_weight = stable * (moved ? 0.24 : 0.52) *
            saturate(1.0 - edge_strength * 0.35);
        resolved = lerp(current, best_history, history_weight);
    }
    const float alpha = InputTexture.Load(int3(dispatch_id.xy, 0)).a;
    OutputTexture[dispatch_id.xy] = float4(AaOutputColor(resolved), alpha);
}

AF4 FsrEasuRF(AF2 p) {
    const AF4 red = InputTexture.GatherRed(LinearClamp, p, int2(0, 0));
    if (Config.x == 0u) return red;
    const AF4 green = InputTexture.GatherGreen(LinearClamp, p, int2(0, 0));
    const AF4 blue = InputTexture.GatherBlue(LinearClamp, p, int2(0, 0));
    return red * rcp(max(max(red, green), blue) + AF4(1.0, 1.0, 1.0, 1.0));
}
AF4 FsrEasuGF(AF2 p) {
    const AF4 green = InputTexture.GatherGreen(LinearClamp, p, int2(0, 0));
    if (Config.x == 0u) return green;
    const AF4 red = InputTexture.GatherRed(LinearClamp, p, int2(0, 0));
    const AF4 blue = InputTexture.GatherBlue(LinearClamp, p, int2(0, 0));
    return green * rcp(max(max(red, green), blue) + AF4(1.0, 1.0, 1.0, 1.0));
}
AF4 FsrEasuBF(AF2 p) {
    const AF4 blue = InputTexture.GatherBlue(LinearClamp, p, int2(0, 0));
    if (Config.x == 0u) return blue;
    const AF4 red = InputTexture.GatherRed(LinearClamp, p, int2(0, 0));
    const AF4 green = InputTexture.GatherGreen(LinearClamp, p, int2(0, 0));
    return blue * rcp(max(max(red, green), blue) + AF4(1.0, 1.0, 1.0, 1.0));
}
AF4 FsrRcasLoadF(ASU2 p) {
    return InputTexture.Load(int3(ASU2(p), 0));
}
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {
    // EASU already writes SRTM working color.  Native-resolution temporal AA
    // writes the original domain, so RCAS maps it here before sharpening.
    if (Config.y != 0u) {
        const AF1 scale = rcp(max(r, max(g, b)) + AF1(1.0));
        r *= scale;
        g *= scale;
        b *= scale;
    }
}

#include "../third_party/fidelityfx-fsr/ffx_fsr1.h"

void RunEasu(int2 position) {
    if (any(uint2(position) >= Config.zw)) return;
    AF3 color;
    FsrEasuF(color, position, Const0, Const1, Const2, Const3);
    const AF2 alpha_uv =
        (AF2(position) + AF2(0.5, 0.5)) * AF2_AU2(Const0.xy) *
        AF2_AU2(Const1.xy);
    const AF1 alpha = InputTexture.SampleLevel(LinearClamp, alpha_uv, 0.0).a;
    OutputTexture[position] = float4(color, alpha);
}

void RunRcas(int2 position) {
    if (any(uint2(position) >= Config.zw)) return;
    AF3 color;
    FsrRcasF(color.r, color.g, color.b, position, Const0);
    if (Config.x != 0u) {
        FsrSrtmInvF(color);
    }
    const AF1 alpha = InputTexture.Load(int3(position, 0)).a;
    OutputTexture[position] = float4(color, alpha);
}

[numthreads(64, 1, 1)]
void CSEasu(uint3 local_id : SV_GroupThreadID, uint3 group_id : SV_GroupID) {
    AU2 position =
        ARmp8x8(local_id.x) + AU2(group_id.x << 4u, group_id.y << 4u);
    RunEasu(position);
    position.x += 8u;
    RunEasu(position);
    position.y += 8u;
    RunEasu(position);
    position.x -= 8u;
    RunEasu(position);
}

[numthreads(64, 1, 1)]
void CSRcas(uint3 local_id : SV_GroupThreadID, uint3 group_id : SV_GroupID) {
    AU2 position =
        ARmp8x8(local_id.x) + AU2(group_id.x << 4u, group_id.y << 4u);
    RunRcas(position);
    position.x += 8u;
    RunRcas(position);
    position.y += 8u;
    RunRcas(position);
    position.x -= 8u;
    RunRcas(position);
}
