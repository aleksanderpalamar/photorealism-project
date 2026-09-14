Texture2D<float4> SourceTexture : register(t0);
SamplerState SourceSampler : register(s0);

cbuffer FxaaBuffer : register(b0)
{
    float2 TexelSize;
    float InputNeedsSrgbDecode;
    float OutputNeedsSrgbEncode;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

static const uint SearchSteps = 10;
static const float SearchStepScale[SearchSteps] =
{
    1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 4.0, 8.0
};

float3 srgb_to_linear(float3 color)
{
    float3 low = color / 12.92;
    float3 high = pow(max((color + 0.055) / 1.055, 0.0), 2.4);
    return lerp(high, low, step(color, 0.04045.xxx));
}

float3 linear_to_srgb(float3 color)
{
    color = max(color, 0.0);
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(color, 1.0 / 2.4) - 0.055;
    return lerp(high, low, step(color, 0.0031308.xxx));
}

float3 sample_color(float2 uv)
{
    float3 color = SourceTexture.SampleLevel(SourceSampler, uv, 0.0).rgb;
    return InputNeedsSrgbDecode > 0.5 ? srgb_to_linear(color) : color;
}

float perceptual_luma(float2 uv)
{
    return sqrt(max(dot(sample_color(uv), float3(0.299, 0.587, 0.114)), 0.0));
}

struct EdgeFrame
{
    bool horizontal;
    float step_length;
    float local_average;
    float gradient;
    float2 origin;
    float2 direction;
};

EdgeFrame edge_frame(float2 uv, float center, float north, float south,
    float east, float west, float corners_west, float corners_east,
    float corners_north, float corners_south)
{
    float horizontal_edge = abs(corners_west - 2.0 * west) +
        2.0 * abs(north + south - 2.0 * center) + abs(corners_east - 2.0 * east);
    float vertical_edge = abs(corners_north - 2.0 * north) +
        2.0 * abs(west + east - 2.0 * center) + abs(corners_south - 2.0 * south);

    EdgeFrame frame;
    frame.horizontal = horizontal_edge >= vertical_edge;
    float before = frame.horizontal ? north : west;
    float after = frame.horizontal ? south : east;
    float gradient_before = before - center;
    float gradient_after = after - center;
    bool steeper_before = abs(gradient_before) >= abs(gradient_after);
    float length_unit = frame.horizontal ? TexelSize.y : TexelSize.x;
    frame.step_length = steeper_before ? -length_unit : length_unit;
    frame.local_average = 0.5 * (center + (steeper_before ? before : after));
    frame.gradient = 0.25 * max(abs(gradient_before), abs(gradient_after));
    float2 half_step = frame.horizontal
        ? float2(0.0, 0.5 * frame.step_length)
        : float2(0.5 * frame.step_length, 0.0);
    frame.origin = uv + half_step;
    frame.direction = frame.horizontal
        ? float2(TexelSize.x, 0.0)
        : float2(0.0, TexelSize.y);
    return frame;
}

float edge_offset(float2 uv, float center, EdgeFrame frame)
{
    float2 negative = frame.origin - frame.direction;
    float2 positive = frame.origin + frame.direction;
    float end_negative = perceptual_luma(negative) - frame.local_average;
    float end_positive = perceptual_luma(positive) - frame.local_average;
    bool done_negative = abs(end_negative) >= frame.gradient;
    bool done_positive = abs(end_positive) >= frame.gradient;

    [loop]
    for (uint index = 1; index < SearchSteps; ++index)
    {
        float2 advance = frame.direction * SearchStepScale[index];
        negative -= done_negative ? 0.0.xx : advance;
        positive += done_positive ? 0.0.xx : advance;
        end_negative = done_negative ? end_negative
            : perceptual_luma(negative) - frame.local_average;
        end_positive = done_positive ? end_positive
            : perceptual_luma(positive) - frame.local_average;
        done_negative = done_negative || abs(end_negative) >= frame.gradient;
        done_positive = done_positive || abs(end_positive) >= frame.gradient;
    }

    float distance_negative = frame.horizontal ? uv.x - negative.x : uv.y - negative.y;
    float distance_positive = frame.horizontal ? positive.x - uv.x : positive.y - uv.y;
    bool negative_closer = distance_negative < distance_positive;
    float closest = min(distance_negative, distance_positive);
    float thickness = max(distance_negative + distance_positive, 1e-6);
    float closest_end = negative_closer ? end_negative : end_positive;
    bool center_below = center < frame.local_average;
    bool consistent = (closest_end < 0.0) != center_below;
    return consistent ? 0.5 - closest / thickness : 0.0;
}

float4 PSFxaa(VertexOutput input) : SV_Target
{
    float2 uv = input.uv;
    float3 source = sample_color(uv);
    float center = perceptual_luma(uv);
    float north = perceptual_luma(uv + float2(0.0, -TexelSize.y));
    float south = perceptual_luma(uv + float2(0.0, TexelSize.y));
    float east = perceptual_luma(uv + float2(TexelSize.x, 0.0));
    float west = perceptual_luma(uv + float2(-TexelSize.x, 0.0));

    float highest = max(center, max(max(north, south), max(east, west)));
    float lowest = min(center, min(min(north, south), min(east, west)));
    float range = highest - lowest;
    float3 result = source;

    if (range >= max(0.0312, highest * 0.125))
    {
        float north_west = perceptual_luma(uv + float2(-TexelSize.x, -TexelSize.y));
        float north_east = perceptual_luma(uv + float2(TexelSize.x, -TexelSize.y));
        float south_west = perceptual_luma(uv + float2(-TexelSize.x, TexelSize.y));
        float south_east = perceptual_luma(uv + float2(TexelSize.x, TexelSize.y));

        EdgeFrame frame = edge_frame(uv, center, north, south, east, west,
            north_west + south_west, north_east + south_east,
            north_west + north_east, south_west + south_east);

        float neighbourhood = (2.0 * (north + south + east + west) +
            north_west + north_east + south_west + south_east) / 12.0;
        float subpixel = smoothstep(0.0, 1.0, saturate(abs(neighbourhood - center) / range));
        float offset = max(edge_offset(uv, center, frame), subpixel * subpixel * 0.75);
        float2 shifted = frame.horizontal
            ? float2(uv.x, uv.y + offset * frame.step_length)
            : float2(uv.x + offset * frame.step_length, uv.y);
        result = sample_color(shifted);
    }

    result = saturate(result);
    return float4(OutputNeedsSrgbEncode > 0.5 ? linear_to_srgb(result) : result, 1.0);
}
