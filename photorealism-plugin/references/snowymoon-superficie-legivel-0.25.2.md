# Superficie legivel do SnowyMoon 2.5.9

Estado: leitura estatica dos dois DLLs do pacote `ets2ats_lighting_v2_5_9_snowymoon.io`
(`dxgi.dll` 18,3 MB, `dinput8.dll` 730 KB). Cobre **so o que e legivel**. A biblioteca de
injecao em HLSL esta em texto puro no binario, e e de onde sai quase tudo abaixo.

Este documento e leitura de comportamento e de contrato de interface. Nao e base para
transcrever codigo: a implementacao do plugin continua saindo de medicao propria, como na
0.25.0 e na 0.25.2.

## O limite: o que nao da para ler

O `dxgi.dll` tem nove secoes, e a ultima e `.vlizer`, 4,9 MB marcada CODE. `.vlizer` e a
assinatura do virtualizador da Oreans (Code Virtualizer / WinLicense). Os strings confirmam
o par licenciador: `Incorrect or expired license key.`, `Invalid license!`. A tabela de
importacao nao sai no `objdump` -- import protegido, do mesmo pacote.

O que esta ali dentro foi convertido para bytecode de uma VM proprietaria: nao existe mais
como x86 legivel. **A chave de serie nao muda isso** -- ela e token de ativacao do
WinLicense, autoriza executar, nao descriptografa a secao. Virtualizado continua
virtualizado com ou sem chave. Tudo neste documento vem do `.text`/`.rdata` legiveis.

## Arquitetura observavel

| Sinal | String | Leitura |
|---|---|---|
| Overlay in-game | `imgui.ini`, `imgui_log.txt`, VS de `ProjectionMatrix`/`VS_INPUT` | Dear ImGui |
| GUI fora do jogo | `wxSystemOptions`, `wxSharedDCBufferManager`, `..\..\include\wx/dcbuffer.h` | wxWidgets |
| Upscaling | `DLSS.Hint.Render.Preset.*`, `DLSSOptimalSettingsCallback`, `RayReconstruction.Hint.Render.Preset.*` | DLSS + Ray Reconstruction (NVIDIA) |
| Distribuicao | `https://cdn.snowymoondl.net/lighting_v2/ets2ats_lighting_v`, `snowymoon.io/?dl=v2` | CDN propria |
| Diagnostico | `snowymoon_logs.txt`, `frame.txt`, `snowymoon_error.zip` | log em arquivo |

O caminho de upscaling dele e DLSS/RR, isto e, NVIDIA. O do plugin e FSR, que roda na RX
6600. Nao ha sobreposicao de escopo aqui.

## O contrato de constantes: b9, b11, b12

Ele declara tres constant buffers proprios. **Nenhum colide com o b13 do plugin.**

`cbuffer cb11 : register(b11)` -- estado curto por quadro:

    float4 vid; float2 wndsize2; uint taaidx; uint empty;

`cbuffer cb12 : register(b12)` -- camera, sol e chuva, na ordem declarada:

    jitter, wndsize, sharpensettings, sharpensettings2, jitterprev,
    invViewMat, invProjMat, prevViewMat, prevProjMat, viewMat, projMat,
    motionblurdata, campos, globalinfo,
    prevInvViewMat, prevInvProjMat,
    sundir, suncolor, skycolor, raininfo

`cbuffer cb9 : register(b9)` -- configuracao vinda do menu, com os comentarios do proprio
autor preservados no binario:

    float4 roadcfg;       // use default normals, normal intensity
    float4 vegetationcfg; // leaves thickness, grass thickness
    int    LightingMethod;
    float3 surfacecfg;
    float4 cb66[2];       // float4 f0, f1

Alem disso amarra `Texture2D<float4> texrainripple : register(t9)` e reusa os samplers
`s0`-`s7` e as texturas `t0/t4/t6/t14` do proprio jogo.

**O achado que mais importa e o b12.** Ele nao le as matrizes do anti-aliasing temporal do
jogo: monta `viewMat`, `projMat`, `invViewMat`, `invProjMat` e as versoes `prev*` por conta
propria e as liga em b12. As funcoes de injecao usam isso direto --
`mul(float4(viewpos,1), invViewMat)` para voltar a posicao de mundo, e `campos.xz` para
distancia. Ou seja: a posicao de mundo por pixel, que a 0.24.1/0.24.2 perseguiu dentro da
captura, aqui e um dado que o proprio modulo fornece, nao que ele extrai do jogo.

## A assinatura real do G-buffer: sete alvos

O template de injecao declara a saida completa do passe `defattr`:

    out float4 o0 : SV_Target0,
    out float4 o1 : SV_Target1,
    out float4 o2 : SV_Target2,
    out uint4  o3 : SV_Target3,
    out float2 o4 : SV_Target4,
    out uint   o5 : SV_Target5,
    out float1 o6 : SV_Target6

A 0.24.0 mediu quatro alvos na captura (bind f10/f10/f10/f12) e a 0.25.0 ligou b13 por essa
forma. **Sao sete**, e os tres ultimos (`o4` float2, `o5` uint, `o6` float1) nunca foram
medidos nem escritos pelo plugin.

Papel dos canais, pelo uso no codigo dele:

| Alvo | Tipo | Uso observado |
|---|---|---|
| `o0.rgb` | float4 | normal em espaco de view (`o0.rgb = viewNormal`) |
| `o1` | float4 | especular/gloss: escreve `o1.b`, `o1.g`, e `o1.a = lerp(o1.a, 1000, o1.b)` |
| `o2.rgb` | float4 | albedo (escurecido pelo molhado) |
| `o3` | uint4 | empacotado: `o3.z = (uint)min(32767, 8192*refam)`, `o3.w` por bitmask, `o3.xy = 0` |
| `o4`/`o5`/`o6` | float2/uint/float1 | declarados, sem escrita no trecho legivel |

O `o3.z` confirma por fora a medicao da 0.24.0: refletividade vive em `o3.z`, aqui como
inteiro em escala de 8192.

## A biblioteca de injecao

Quatro funcoes legiveis, em ordem no binario:

- `calcripple(wpos, wetness)` -- ondulacao de chuva. **Nao e procedural**: amostra a textura
  `texrainripple` (t9) duas vezes, em `frac(wpos.xz/3)` e `frac((wpos.xz+1.11)/2.5)`, cada
  uma com fase propria animada por `motionblurdata.a`, e combina as duas normais. O plugin
  gera a ondulacao por ruido no shader; ele usa tabela.
- `ComputeLuminance(color)` -- `dot(color, float3(0.299, 0.587, 0.114))`.
- `calcnormalmap(wpos, dist, colorTexture, sampler, uv)` -- normal de detalhe por gradiente
  de luminancia do albedo, com `strength`, `maxGradient = 0.5`, `aoStrength`, `aoSpread` e
  `GetDimensions` para escolher o nivel de mip. Mesma ideia da 0.25.0 do plugin.
- `apply_rain(...)` -- piso molhado. Deriva `wetness` do gloss e de `raininfo.g`, multiplica
  por `raininfo.r` e pelo fator de reflexo, soma a ondulacao, monta TBN por `ddx/ddy` da
  posicao de mundo e escreve normal, albedo escurecido e os canais de gloss.
- `apply_road1(...)` / `apply_road2(...)` -- uma e duas camadas de albedo, com mistura.

`roadcfg` e usado em dois pontos: `roadcfg.y` modula a intensidade das normais
(`normalmap.xy *= lerp(roadcfg.y, 1, wetam*refam)`) e `roadcfg.x` faz voltar a normal
padrao (`lerp(normalmap1, defnormalmap1, roadcfg.x)`). Isto casa com o par "Normais padrao"
e "Intensidade das normais" que a 0.25.0 tirou de pendente -- o mapeamento do plugin esta
certo.

## Como ele liga o shader: DEF_* e SHADERTYPE

Este e o contraste de arquitetura, e e o ponto que separa os dois plugins.

Ele emite um bloco de macros por shader, preenchido com os registradores daquela variante:

    #define DEF_INPUT / DEF_NORMAL v / DEF_VIEWPOS v / DEF_UV1 v / DEF_UV2 v
    #define DEF_SAMPLER_ALB1 s / DEF_SAMPLER_ALB2 s
    #define DEF_TINT_ALB1 cb / DEF_TINT_ALB2 cb / DEF_TINT_SPEC1 cb / DEF_TINT_SPEC2 cb
    #define DEF_RAIN1 cb0[ / DEF_RAIN2 cb / DEF_BLEND v
    #define SHADERTYPE <n>

E `SHADERTYPE` seleciona o corpo. Pelos sitios de chamada, o eixo de camadas e claro:

| SHADERTYPE | Camadas | Chama |
|---|---|---|
| 0, 1 | uma camada de albedo | `apply_road1` |
| 2, 3 | duas camadas + `blendamount` | `apply_road2` |

O segundo eixo (`#if SHADERTYPE == 1 \|\| SHADERTYPE == 3`) guarda ajustes extras dentro das
funcoes; os blocos que sobraram nesses guardas estao comentados no binario.

**Ele reescreve o corpo do shader, nao acrescenta ao fim.** Dentro dos ramos ele re-emite a
propria conta do jogo -- `r0 = t6.Sample(DEF_SAMPLER_ALB1, DEF_UV1); o1.xyz = r2.xyz *
r1.xyz; o2.xyz = r2.xyz * r0.xyz;` -- porque precisa dos valores intermediarios. Isso exige
saber, variante a variante, em que registrador estao normal, viewpos, UV, tint e blend. E
exatamente a "ligacao por variante" que a 0.25.0 evitou: o plugin roda **depois** do codigo
do jogo e le `o0`/`o3` ja escritos, sem precisar conhecer o layout de cada variante.

O custo da escolha dele aparece na manutencao: alem das macros por variante, ele ancora seis
alvos por caminho fixo `/effect/eut2/_shd/<md5>.sm5x.fso`
(`12e7a8c1...`, `30e49f0e...`, `51ce0a3a...`, `917acb1a...`, `bea10f22...`, `cbfb652d...`).
Esses seis nao existem no `effect.scs` do ETS2 nem do ATS atuais -- e a mesma constatacao da
0.25.2, agora com os hashes a vista.

## Chaves de `config.cfg` presentes no binario

As oito que a 0.25.1 ja media (`r_aa`, `r_ssao`, `r_dof`, `r_normal_maps`, `r_fake_shadows`,
`r_color_correction`, `g_veg_detail`, `g_reflection`) estao la. Continuam **ausentes** as
que a 0.25.1 listou como inexistentes (`r_texture_detail`, `r_anisotropy_factor`,
`r_sun_shadow_quality`, `r_interior_shadow`, `r_far_shadow_disable`, `r_deferred_mirrors`,
`r_mirror_view_distance`, `r_sunshafts`, `g_grass_density`, `g_pedestrian`, `r_scale_x`,
`g_gfx_quality`) -- a conclusao daquela versao se sustenta.

Aparecem, porem, chaves que nao estavam no levantamento anterior:

    g_rain_reflection, g_rain_reflect_actor, g_rain_reflect_cache,
    g_rain_reflect_hookups, g_rain_reflect_traffic,
    g_additional_water_fov, g_truck_light_specular,
    r_fullscreen_borderless, r_windowed_borderless

A familia `g_rain_reflect_*` interessa diretamente ao modulo de piso molhado da 0.25.0: sao
os reflexos de chuva do proprio jogo, que hoje o plugin nao toca.

## Rotulos do menu dele

`Global Quality`, `Quality High/Medium/Low`, `Details High/Low`,
`Default Rain Enabled/Disabled`, `Low Resolution SSAO Enabled/Disabled`, e secoes
`## Exposure`, `## Night Exposure`, `## Pre-Exposure`, `## Interior Intensity`,
`## Normals Intensity`.

## O que isto muda para o plugin

1. **Sete alvos, nao quatro.** A elegibilidade da 0.25.0 aceita o bind de quatro alvos. Vale
   medir numa captura se o `defattr` do ETS2 realmente expoe `o4`/`o5`/`o6`, e o que ha
   neles, antes de supor que os quatro bastam.
2. **`o1` e o alvo de gloss/rugosidade.** O plugin escreve `o0` (normal) e `o2` (albedo) e
   nao mexe em `o1`. Molhado convincente passa por `o1`, que e o que ele faz.
3. **Reflexo de chuva nativo.** `g_rain_reflect_*` e um caminho de imagem que o plugin
   ignora hoje.
4. **Posicao de mundo por pixel.** Ele resolve com matrizes proprias em b12 em vez de
   extrair as do jogo. Se o plugin seguir o mesmo caminho, o motion blur pendente deixa de
   depender de achar as matrizes do TAA na captura.

## O que NAO esta visivel, e nao deve ser afirmado

- **`vegetationcfg` so aparece declarado.** O comentario diz `leaves thickness, grass
  thickness`, mas **nenhum trecho legivel le esse campo** -- ao contrario de `roadcfg`, que
  tem dois usos rastreaveis. O mesmo vale para `surfacecfg` e `LightingMethod`: declarados,
  nunca usados no que da para ler. Como ele separa grama de folha continua **desconhecido**;
  o `SHADERTYPE` que da para ver escolhe numero de camadas de albedo, nao tipo de vegetacao.
  A pendencia de `vegetation_grass_thickness` na 0.25.2 segue sem resposta por esta via.
- Todo o restante do modulo (iluminacao, exposicao, bloom, o SSAO dele, a logica de licenca)
  esta na secao virtualizada e nao foi lido.
