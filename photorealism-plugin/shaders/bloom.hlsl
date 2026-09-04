Texture2D SourceTexture : register(t0);
SamplerState SourceSampler : register(s0);

cbuffer BloomBuffer : register(b0)
{
    float2 SourceTexelSize;
    float2 FilterRadius;

    float Threshold;
    float Knee;
    float InputNeedsSrgbDecode;
    float OutputNeedsSrgbEncode;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// O glow largo nao vem de um kernel grande, e sim de uma piramide.
//
// Um separavel de nove taps a meia resolucao alcanca cerca de 8 pixels de
// sigma -- 0,007 da altura em 1080p. As referencias do ATS mostram flare de
// sol na casa de centesimos da altura, uma ordem de grandeza acima, e chegar
// la esticando os offsets do kernel deixa buracos entre as amostras: o glow
// vira anel em vez de queda suave.
//
// Cada nivel da piramide dobra o alcance pelo mesmo custo, porque o numero de
// pixels cai pela metade junto. Por isso o alcance aqui e escolhido pela
// CONTAGEM DE NIVEIS e nao pelo tamanho do filtro, e por isso o raio medido
// nas referencias vira `level_count` no C++ em vez de virar um multiplicador.

float3 decode(float3 color)
{
    if (InputNeedsSrgbDecode > 0.5)
    {
        float3 low = color / 12.92;
        float3 high = pow(max((color + 0.055) / 1.055, 0.0), 2.4);
        return lerp(high, low, step(color, 0.04045.xxx));
    }
    return color;
}

float3 sample_source(float2 uv)
{
    return decode(SourceTexture.Sample(SourceSampler, uv).rgb);
}

// Meia resolucao por passo, com quatro taps bilineares a meio texel do
// centro. Cada tap ja media quatro texels da origem, entao os quatro cobrem
// dezesseis -- o suficiente para um destaque de um pixel nao cintilar quando
// a camera anda.
float3 box_downsample(float2 uv)
{
    float2 offset = SourceTexelSize * 0.5;
    return 0.25 * (
        sample_source(uv + float2(-offset.x, -offset.y)) +
        sample_source(uv + float2( offset.x, -offset.y)) +
        sample_source(uv + float2(-offset.x,  offset.y)) +
        sample_source(uv + float2( offset.x,  offset.y)));
}

// Limiar de joelho suave.
//
// Um corte reto faz o glow PISCAR: um destaque que oscila em volta do limiar
// entra e sai inteiro a cada frame. Aqui a contribuicao sobe de forma
// quadratica ao longo da largura do joelho, entao atravessar o limiar e
// continuo.
//
// A contribuicao e exatamente ZERO abaixo de (threshold - knee). Isso importa
// mais do que parece: sem esse zero exato, todo pixel da cena contribui um
// pouco e o bloom vira veu cinza uniforme em vez de brilho em volta de
// fontes. tests/bloom_curve_test.cpp guarda essa propriedade.
//
// Threshold e Knee chegam em sRGB 0-1, que e a unidade que bloom_report.py
// imprime (dividida por 255) e a unidade em que o olho julga "isso e claro".
// A comparacao acontece em linear, entao a conversao e aqui -- assim o numero
// medido entra no cfg sem nenhuma conta intermediaria escondida.
float srgb_to_linear_scalar(float value)
{
    if (value <= 0.04045)
    {
        return value / 12.92;
    }
    return pow(max((value + 0.055) / 1.055, 0.0), 2.4);
}

float3 apply_threshold(float3 color)
{
    float threshold = srgb_to_linear_scalar(Threshold);
    float knee = max(srgb_to_linear_scalar(Threshold + Knee) - threshold,
                     0.0001);

    float brightness = max(color.r, max(color.g, color.b));
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);

    float contribution =
        max(soft, brightness - threshold) / max(brightness, 0.0001);
    return color * max(contribution, 0.0);
}

// Nao ha vertex shader aqui: os tres passes reusam o VSMain de
// photorealism.hlsl, como ssao.hlsl e temporal.hlsl ja fazem. A assinatura e a
// mesma e o triangulo de tela cheia e o mesmo.

// Cena em resolucao cheia -> nivel 0 da piramide, ja limiarizado.
float4 PSBloomBright(VertexOutput input) : SV_Target
{
    return float4(apply_threshold(box_downsample(input.uv)), 1.0);
}

// Nivel i -> nivel i+1. Sem limiar: quem nao passou no nivel 0 nao volta.
float4 PSBloomDownsample(VertexOutput input) : SV_Target
{
    return float4(box_downsample(input.uv), 1.0);
}

// Nivel i+1 -> nivel i, somado por blend aditivo.
//
// O filtro e um tent 3x3 nos texels do nivel MENOR, que e o que espalha: a
// interpolacao bilinear da subida ja suaviza, e o tent tira a aresta que
// sobra. Somar em vez de substituir e o que faz a piramide inteira empilhar
// num unico glow com queda suave.
float4 PSBloomUpsample(VertexOutput input) : SV_Target
{
    float2 offset = SourceTexelSize * FilterRadius;
    float3 total =
        sample_source(input.uv + float2(-offset.x,  offset.y)) * 1.0 +
        sample_source(input.uv + float2(       0.0,  offset.y)) * 2.0 +
        sample_source(input.uv + float2( offset.x,  offset.y)) * 1.0 +
        sample_source(input.uv + float2(-offset.x,       0.0)) * 2.0 +
        sample_source(input.uv                                ) * 4.0 +
        sample_source(input.uv + float2( offset.x,       0.0)) * 2.0 +
        sample_source(input.uv + float2(-offset.x, -offset.y)) * 1.0 +
        sample_source(input.uv + float2(       0.0, -offset.y)) * 2.0 +
        sample_source(input.uv + float2( offset.x, -offset.y)) * 1.0;
    float3 result = total * (1.0 / 16.0);

    // Zero nos passes da piramide, que escrevem em alvos sRGB e portanto
    // codificam sozinhos. Vale 1 apenas no preview do Insert, quando o RTV
    // sRGB do backbuffer nao pode ser criado e a codificacao volta a ser
    // manual -- sem isto o preview sairia escuro justamente no caminho de
    // fallback, que e onde ninguem olha ate quebrar.
    if (OutputNeedsSrgbEncode > 0.5)
    {
        float3 low = result * 12.92;
        float3 high = 1.055 * pow(max(result, 0.0), 1.0 / 2.4) - 0.055;
        result = lerp(high, low, step(result, 0.0031308.xxx));
    }
    return float4(result, 1.0);
}
