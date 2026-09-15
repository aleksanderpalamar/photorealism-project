Texture2D<float4> SourceTexture : register(t0);

cbuffer PreToneBuffer : register(b0)
{
    float ExposureGain;
    float Contrast;
    float2 PreTonePadding;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

static const float Pivot = 0.18;
static const float MaximumHdr = 60000.0;

float4 PSPreTone(VertexOutput input) : SV_Target
{
    float4 source = SourceTexture.Load(int3(input.position.xy, 0));
    float3 color = max(source.rgb, 0.0) * ExposureGain;
    float luma = max(dot(color, float3(0.2126, 0.7152, 0.0722)), 1e-6);
    float exponent = max(1.0 + Contrast, 0.1);
    float shaped = Pivot * pow(luma / Pivot, exponent);
    color *= shaped / luma;
    return float4(min(color, MaximumHdr), source.a);
}
