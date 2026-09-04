Texture2D SceneTexture : register(t0);
Texture2D BloomTexture : register(t1);
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

    float3 BlackLift;
    float HighlightRolloff;

    float Tint;
    float BloomEnabled;
    float BloomIntensity;
    float BloomPadding0;
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
// Balanco de branco NORMALIZADO em luminancia -- 0.18.2.
//
// Ate a 0.18.1 esta funcao devolvia `color * balance` cru, e o vetor de
// balanco carregava exposicao junto com a cor. No perfil aprovado
// (6400K, tint 0,50) o balanco e 0,9773/1,0500/0,9721, cuja luminancia
// Rec.709 e 1,028920: **+0,0411 EV que ninguem pediu**. Contra o
// `exposure=-0,030` do cfg, a exposicao efetiva era +0,0111 -- com o sinal
// trocado em relacao ao que o arquivo e o log diziam.
//
// Isso nao e so contabilidade. O eixo verde-magenta e o que mais mexe na
// luminancia, porque G pesa 0,7152 dos tres: varrer tint de 0,0 a 1,0
// desloca a imagem em 0,0803 EV, ou 5,7% de brilho. Enquanto tint era uma
// constante isso era um erro fixo, absorvido na calibracao sem que nada
// acusasse. A partir da 0.19.0 tint passa a se mover com o clima, e o erro
// passa a se mover junto: a imagem clarearia ao ficar esverdeada e escureceria
// ao esfriar, sozinha. Cor que muda brilho e exatamente o que se le como
// irreal.
//
// Dividir pela propria luminancia deixa o vetor puramente cromatico, e ai
// `exposure` volta a ser a unica coisa que controla brilho. A compensacao de
// +0,0411 EV foi para a exposicao base do cfg, entao a saida de hoje nao muda
// um codigo -- os dois sao multiplicacoes em linear e comutam.
//
// O max() nunca morde: a luminancia do balanco fica entre 0,95 e 1,05 em todo
// o dominio de Temperature e Tint. Esta ali para que um perfil futuro absurdo
// nao vire divisao por zero.
float3 apply_temperature(float3 color)
{
    float shift = clamp((Temperature - 6500.0) / 3500.0, -1.0, 1.0);
    float tint = clamp(Tint, -1.0, 1.0);
    float3 balance = float3(
        1.0 - 0.08 * shift - 0.05 * tint,
        1.0 + 0.10 * tint,
        1.0 + 0.10 * shift - 0.05 * tint);
    return color * (balance / max(luminance(balance), 1e-4));
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

// Toe: o piso do preto, POR CANAL desde a 0.17.1.
//
// Medido nas referencias, o 1% mais escuro fica em 8-11 de 255 nas cinco --
// nada e esmagado a zero. O plugin batia em 0 nas tres capturas da 0.13.3, e
// era por isso que o painel virava massa preta enquanto o da referencia, mais
// escuro na mediana, deixava ler cada manometro.
//
// Escalar, porem, o piso sai acromatico, e o alvo NAO e acromatico. O 1% mais
// escuro das cinco referencias, em 255:
//
//   encoberto 2,1/5,8/5,2   crepusculo 1,6/5,7/5,1   sol 3,8/7,2/7,6
//   neblina   5,8/9,1/10,6  neblina    5,7/9,0/10,5
//
// R fica entre 29% e 64% de G, e B acima de G nas de neblina. Nas capturas da
// 0.17.0 o piso saiu 8/8/8 e 9/9/9 -- R/G e B/G exatamente 1,000 -- porque
// este lift era um float. temperature e tint nao alcancam isso: os dois
// multiplicam a faixa inteira, e o topo ja esta certo (R/G medido 0,955-1,002
// na referencia contra 0,945-0,958 aqui).
float3 apply_black_lift(float3 color, float3 lift)
{
    float3 floor_value = clamp(lift, 0.0, 0.02);
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

    // Contraste em torno do pivo, em POTENCIA e nao em reta -- 0.17.1.
    //
    // Ate a 0.17.0 esta linha era max((color - pivot) * Contrast + pivot, 0.0),
    // e era ELA, e nao o black_lift, que destruia a sombra. Com Contrast acima
    // de 1 a reta manda todo valor linear abaixo de pivot*(Contrast-1)/Contrast
    // para negativo, e o max() grampeia o conjunto inteiro no mesmo zero. Com o
    // perfil aprovado (pivo 0,18, Contrast 1,07) esse limiar e 0,01178 na
    // entrada DESTE passo; contadas a exposicao e o ganho de sombra que vem
    // antes, e 0,0147 na entrada da cadeia, ou 32 em 255 no encode -- a cabine
    // inteira. O black_lift depois so escolhia QUAL valor essa massa
    // receberia.
    //
    // Medido nas quatro capturas da 0.17.0: 72 a 90% dos pixels escuros com os
    // tres canais EXATAMENTE iguais, e 12 a 13 niveis distintos abaixo de
    // 12/255, contra 24 a 31 nas cinco referencias. Nao era falta de piso, era
    // falta de estrutura para o piso sustentar.
    //
    // A potencia tem o mesmo pivo e praticamente a mesma inclinacao perto dele,
    // mas manda 0 para 0 em vez de para negativo, e e monotonica em todo o
    // dominio. O mesmo degrade de entrada 0-40 devolve 30 niveis distintos em
    // vez de 10. O epsilon existe so para nao passar zero exato ao pow, que o
    // compila como exp2(y*log2(x)) e faria log2(0).
    const float pivot = 0.18;
    color = pivot * pow(max(color, 1e-6) / pivot, Contrast);

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

    // O bloom entra AQUI, e a posicao e as tres coisas de uma vez.
    //
    // Depois do realce, senao o sharpening morde a borda do glow e devolve um
    // halo duplo -- realcar uma coisa que ja e suave por natureza.
    //
    // Antes dos controles tonais, para o brilho receber exposicao, contraste,
    // temperatura e tint junto com o resto. O flare do sol na golden hour tem
    // que sair QUENTE porque a imagem inteira e quente, e nao cinza colado por
    // cima.
    //
    // E portanto antes de apply_highlight_rolloff, que comprime a soma em vez
    // de deixar estourar. O ombro da 0.14.0 e o que torna somar luz aqui
    // seguro; sem ele isto seria um plato branco.
    //
    // Com BloomEnabled em zero nada e somado e a saida e identica a 0.16.0,
    // pixel a pixel. E o que torna o modulo desligavel de verdade.
    if (BloomEnabled > 0.5)
    {
        float3 bloom = BloomTexture.Sample(SceneSampler, input.uv).rgb;
        center += bloom * BloomIntensity;
    }

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
