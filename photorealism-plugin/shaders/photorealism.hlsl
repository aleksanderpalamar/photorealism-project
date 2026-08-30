Texture2D SceneTexture : register(t0);
SamplerState SceneSampler : register(s0);

cbuffer SettingsBuffer : register(b0)
{
    float2 TexelSize;
    float Exposure;
    float Temperature;

    float Contrast;
    float Saturation;
    float Vibrance;
    float Shadows;

    float Highlights;
    float Blacks;
    float Whites;
    float LocalContrast;

    float Sharpness;
    float Vignette;
    float InputNeedsSrgbDecode;
    float OutputNeedsSrgbEncode;

    float BlackLift;
    float HighlightRolloff;
    float Tint;
    float VisualPadding;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(uint vertex_id : SV_VertexID)
{
    VertexOutput output;
    output.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

float luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 srgb_to_linear(float3 color)
{
    float3 low = color / 12.92;
    float3 high = pow(max((color + 0.055) / 1.055, 0.0), 2.4);
    float3 use_low = step(color, 0.04045.xxx);
    return lerp(high, low, use_low);
}

float3 linear_to_srgb(float3 color)
{
    color = max(color, 0.0);
    float3 low = color * 12.92;
    float3 high = 1.055 * pow(color, 1.0 / 2.4) - 0.055;
    float3 use_low = step(color, 0.0031308.xxx);
    return lerp(high, low, use_low);
}

float4 sample_scene(float2 uv)
{
    float4 sample_value = SceneTexture.Sample(SceneSampler, uv);
    if (InputNeedsSrgbDecode > 0.5)
    {
        sample_value.rgb = srgb_to_linear(sample_value.rgb);
    }
    return sample_value;
}

// Balanco de branco em dois eixos, como uma camera tem.
//
// Ate a 0.13.3 existia so o eixo de temperatura, e ele troca R contra B
// deixando G intocado -- o eixo verde-magenta simplesmente nao existia. As
// quatro referencias do ATS medidas na 0.14.0 tem G como canal mais alto, e
// nenhuma combinacao de temperatura alcanca isso: ela move os dois canais
// errados. Tint move o terceiro.
//
// A compensacao em R e B e metade do ganho de G para que empurrar o tint mude
// a cor sem mudar junto o brilho, e a exposicao nao precise ser recalibrada a
// cada ajuste.
float3 apply_temperature(float3 color)
{
    float shift = clamp((Temperature - 6500.0) / 3500.0, -1.0, 1.0);
    float tint = clamp(Tint, -1.0, 1.0);
    float3 balance = float3(
        1.0 - 0.08 * shift - 0.05 * tint,
        1.0 + 0.10 * tint,
        1.0 + 0.10 * shift - 0.05 * tint);
    return color * balance;
}

// Ombro: os altos comprimem para 1 em vez de bater nele.
//
// Ate a 0.13.3 o unico limite era o saturate() do fim de PSMain, que e um
// corte reto -- o ceu e o capo branco viravam chapada sem gradacao. A
// exponencial abaixo tem derivada continua no joelho e nunca alcanca 1, entao
// o saturate posterior deixa de ter o que cortar.
//
// strength=0 devolve a curva antiga, o que mantem o modulo desligavel.
float3 apply_highlight_rolloff(float3 color, float strength)
{
    float amount = saturate(strength);
    if (amount <= 0.0)
    {
        return color;
    }

    // Joelho entre 1.0 (sem ombro) e 0.5 (ombro longo).
    float knee = lerp(1.0, 0.5, amount);
    float headroom = max(1.0 - knee, 0.0001);
    float3 excess = max(color - knee, 0.0.xxx);
    float3 compressed = knee + headroom * (1.0 - exp(-excess / headroom));
    return min(color, compressed);
}

// Toe: o piso do preto.
//
// Medido nas referencias, o 1% mais escuro fica em 8-11 de 255 nas quatro --
// nada e esmagado a zero. O plugin batia em 0 nas tres capturas da 0.13.3, e
// era por isso que o painel virava massa preta enquanto o da referencia, mais
// escuro na mediana, deixava ler cada manometro.
//
// A conta e exata: em linear, lift=0.0027 leva o preto a 0.0027*12.92 =
// 0.0349 em sRGB, ou 8,9 em 255. O alvo nao e vago, e um parametro.
float3 apply_black_lift(float3 color, float lift)
{
    float floor_value = clamp(lift, 0.0, 0.02);
    return floor_value + (1.0 - floor_value) * color;
}

float3 apply_tonal_controls(float3 color)
{
    color = max(color * exp2(Exposure), 0.0);
    color = apply_temperature(color);

    float initial_luma = luminance(color);
    float shadow_mask = 1.0 - smoothstep(0.025, 0.35, initial_luma);
    float highlight_mask = smoothstep(0.45, 1.0, initial_luma);
    color *= exp2(Shadows * shadow_mask + Highlights * highlight_mask);

    float black_mask = 1.0 - smoothstep(0.0, 0.16, initial_luma);
    float white_mask = smoothstep(0.55, 1.0, initial_luma);
    color += Blacks * black_mask * 0.06;
    color += Whites * white_mask * 0.06;

    const float pivot = 0.18;
    color = max((color - pivot) * Contrast + pivot, 0.0);

    float luma = luminance(color);
    color = lerp(luma.xxx, color, Saturation);

    float maximum = max(color.r, max(color.g, color.b));
    float minimum = min(color.r, min(color.g, color.b));
    float chroma = saturate(maximum - minimum);
    float vibrance_factor = 1.0 + Vibrance * (1.0 - chroma);
    color = lerp(luminance(color).xxx, color, vibrance_factor);
    return max(color, 0.0);
}

float4 PSMain(VertexOutput input) : SV_Target
{
    float4 source = sample_scene(input.uv);
    float3 center = source.rgb;

    float3 neighbours =
        sample_scene(input.uv + float2(TexelSize.x, 0.0)).rgb +
        sample_scene(input.uv - float2(TexelSize.x, 0.0)).rgb +
        sample_scene(input.uv + float2(0.0, TexelSize.y)).rgb +
        sample_scene(input.uv - float2(0.0, TexelSize.y)).rgb;
    float3 blur = neighbours * 0.25;
    float3 detail = center - blur;
    float edge = abs(luminance(center) - luminance(blur));
    float edge_mask = smoothstep(0.0015, 0.08, edge);
    float center_luma = luminance(center);
    float midtone_mask =
        smoothstep(0.03, 0.18, center_luma) *
        (1.0 - smoothstep(0.55, 0.90, center_luma));
    center += detail *
        (Sharpness + LocalContrast * edge_mask * midtone_mask);

    float3 color = apply_tonal_controls(center);

    // O ombro entra aqui, comprimindo o que a exposicao e o contraste
    // produziram, e antes da vignette -- que e efeito de lente e escurece luz,
    // nao codigo de saida.
    color = apply_highlight_rolloff(color, HighlightRolloff);

    float2 vignette_uv = input.uv * (1.0 - input.uv);
    float vignette_shape = saturate(vignette_uv.x * vignette_uv.y * 16.0);
    color *= lerp(1.0, smoothstep(0.0, 1.0, vignette_shape), Vignette);

    // O lift e a ULTIMA coisa antes do encode, e nao um passo do meio: e o
    // piso de preto da imagem final, como a densidade minima de uma copia em
    // filme. Aplicado antes da vignette, os cantos escureceriam abaixo dele e
    // o piso deixaria de ser piso.
    color = apply_black_lift(color, BlackLift);

    color = saturate(color);
    if (OutputNeedsSrgbEncode > 0.5)
    {
        color = linear_to_srgb(color);
    }

    return float4(saturate(color), source.a);
}
