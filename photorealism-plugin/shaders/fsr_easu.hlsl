Texture2D<float4> InputTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer EasuBuffer : register(b0)
{
    float2 InputSize;
    float2 OutputSize;
    float2 InputTexelSize;
    float2 EasuPadding;
};

float tap_luma(float3 color)
{
    return color.g;
}

float3 load_input(int2 position)
{
    int2 clamped = clamp(position, int2(0, 0), int2(InputSize) - 1);
    return InputTexture.Load(int3(clamped, 0)).rgb;
}

void accumulate_direction(
    inout float2 direction,
    inout float length_estimate,
    float weight,
    float above,
    float left,
    float center,
    float right,
    float below)
{
    float horizontal_a = right - center;
    float horizontal_b = center - left;
    float horizontal_span = max(abs(horizontal_a), abs(horizontal_b));
    float horizontal_dir = right - left;

    float vertical_a = below - center;
    float vertical_b = center - above;
    float vertical_span = max(abs(vertical_a), abs(vertical_b));
    float vertical_dir = below - above;

    direction.x += horizontal_dir * weight;
    direction.y += vertical_dir * weight;

    float horizontal_len =
        saturate(abs(horizontal_dir) / max(horizontal_span, 1.0 / 32768.0));
    float vertical_len =
        saturate(abs(vertical_dir) / max(vertical_span, 1.0 / 32768.0));
    horizontal_len *= horizontal_len;
    vertical_len *= vertical_len;
    length_estimate += max(horizontal_len, vertical_len) * weight;
}

void accumulate_tap(
    inout float3 accumulated,
    inout float accumulated_weight,
    float2 offset,
    float2 direction,
    float2 stretch,
    float lobe,
    float clip,
    float3 color)
{
    float2 rotated;
    rotated.x = offset.x * direction.x + offset.y * direction.y;
    rotated.y = offset.x * -direction.y + offset.y * direction.x;
    rotated *= stretch;

    float distance_squared = min(dot(rotated, rotated), clip);
    float window = (2.0 / 5.0) * distance_squared - 1.0;
    float lanczos = lobe * distance_squared - 1.0;
    window *= window;
    lanczos *= lanczos;
    window = (25.0 / 16.0) * window - (25.0 / 16.0 - 1.0);

    float weight = window * lanczos;
    accumulated += color * weight;
    accumulated_weight += weight;
}

[numthreads(8, 8, 1)]
void CSEasu(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= uint(OutputSize.x) || id.y >= uint(OutputSize.y))
    {
        return;
    }

    float2 output_center = (float2(id.xy) + 0.5) / OutputSize;
    float2 input_position = output_center * InputSize - 0.5;
    int2 base = int2(floor(input_position));
    float2 phase = input_position - float2(base);

    float3 taps[12];
    taps[0] = load_input(base + int2(0, -1));
    taps[1] = load_input(base + int2(1, -1));
    taps[2] = load_input(base + int2(-1, 0));
    taps[3] = load_input(base + int2(0, 0));
    taps[4] = load_input(base + int2(1, 0));
    taps[5] = load_input(base + int2(2, 0));
    taps[6] = load_input(base + int2(-1, 1));
    taps[7] = load_input(base + int2(0, 1));
    taps[8] = load_input(base + int2(1, 1));
    taps[9] = load_input(base + int2(2, 1));
    taps[10] = load_input(base + int2(0, 2));
    taps[11] = load_input(base + int2(1, 2));

    float luma[12];
    for (int index = 0; index < 12; ++index)
    {
        luma[index] = tap_luma(taps[index]);
    }

    float2 direction = float2(0.0, 0.0);
    float length_estimate = 0.0;
    accumulate_direction(
        direction, length_estimate, (1.0 - phase.x) * (1.0 - phase.y),
        luma[0], luma[2], luma[3], luma[4], luma[7]);
    accumulate_direction(
        direction, length_estimate, phase.x * (1.0 - phase.y),
        luma[1], luma[3], luma[4], luma[5], luma[8]);
    accumulate_direction(
        direction, length_estimate, (1.0 - phase.x) * phase.y,
        luma[3], luma[6], luma[7], luma[8], luma[10]);
    accumulate_direction(
        direction, length_estimate, phase.x * phase.y,
        luma[4], luma[7], luma[8], luma[9], luma[11]);

    float direction_squared = dot(direction, direction);
    bool flat = direction_squared < (1.0 / 32768.0);
    direction = flat ? float2(1.0, 0.0) : direction * rsqrt(direction_squared);

    float shaped = length_estimate * 0.5;
    shaped = saturate(shaped * shaped);

    float stretch_factor =
        dot(direction, direction) / max(abs(direction.x), abs(direction.y));
    float2 stretch = float2(
        1.0 + (stretch_factor - 1.0) * shaped, 1.0 - 0.5 * shaped);
    float lobe = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * shaped;
    float clip = 1.0 / lobe;

    float2 offsets[12] = {
        float2(0.0, -1.0), float2(1.0, -1.0),
        float2(-1.0, 0.0), float2(0.0, 0.0), float2(1.0, 0.0), float2(2.0, 0.0),
        float2(-1.0, 1.0), float2(0.0, 1.0), float2(1.0, 1.0), float2(2.0, 1.0),
        float2(0.0, 2.0), float2(1.0, 2.0)};

    float3 accumulated = float3(0.0, 0.0, 0.0);
    float accumulated_weight = 0.0;
    float3 lowest = taps[3];
    float3 highest = taps[3];
    for (int tap = 0; tap < 12; ++tap)
    {
        accumulate_tap(
            accumulated, accumulated_weight, offsets[tap] - phase, direction,
            stretch, lobe, clip, taps[tap]);
    }
    lowest = min(min(taps[3], taps[4]), min(taps[7], taps[8]));
    highest = max(max(taps[3], taps[4]), max(taps[7], taps[8]));

    float3 resolved = accumulated / max(accumulated_weight, 1.0 / 32768.0);
    OutputTexture[id.xy] = float4(clamp(resolved, lowest, highest), 1.0);
}
