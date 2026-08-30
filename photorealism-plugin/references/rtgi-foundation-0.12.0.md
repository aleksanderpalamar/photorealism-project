# RTGI 0.12.0 - fundacao do Screen-Space Ray-Traced Global Illumination

Data: 2026-08-28
Escopo: Photorealism 0.12.0, branch `feat/screen-space-ray-traced-global-ilumination`
Estado: andaime entregue; nenhum raio e tracado nesta versao

## Objetivo da tecnica

Aproximar iluminacao global em screen-space reaproveitando o que o plugin ja
descobriu: depth linearizado com reversed-Z, normais reconstruidas, scene color
e historico temporal. O alvo visual e `color bleeding` de curto/medio alcance
(0.5 m a 15 m) sobre cabine, caminhao, asfalto, paredes, postos, edificios e
vegetacao proxima.

O nome e deliberado: **screen-space**, nao hardware ray tracing. A RX 6600 tem
RT cores, mas usa-los exigiria DXR/D3D12 e uma acceleration structure da cena
inteira. Isso fica explicitamente fora do escopo.

## O que esta versao entrega

Consolidacao e andaime. A fase 0.12.0 do documento original era "Normal
reconstruction + debug view", mas o levantamento mostrou que as duas ja
existiam - e triplicadas.

| funcao | ssao.hlsl | temporal.hlsl | depth-preview.hlsl |
|---|---|---|---|
| `linearize_reversed_depth` | sim | sim | sim |
| `reconstruct_view_position` | sim | - | sim |
| `reconstruct_view_normal` | sim, com validade | - | sim, sem validade |
| `linear_to_srgb` | sim | sim | sim |
| `srgb_to_linear` | sim | - | sim |

O preview do Insert ja tinha o modo 4 `reconstructed-normals`, e
`ProjectionScale` ja era derivado do `vertical_fov`. O SSRTGI precisa
exatamente dessas funcoes; uma quarta copia seria insustentavel.

Entregue:

- `shaders/depth_view_space.hlsli` como fonte unica da matematica
  depth -> view-space -> normal. Tudo que antes vinha de cbuffer
  (`NearPlane`, `ProjectionScale`, `TexelSize`) passou a ser parametro
  explicito, e as amostras de depth chegam prontas - cada shader liga a textura
  em um registrador diferente, entao o header faz matematica e o shader faz I/O;
- `src/rtgi_config.hpp`, header-folha puro com os sete modos de `rtgi_debug`,
  a derivacao de resolucao e o clamp dos parametros, testado no Linux com
  `tests/rtgi_config_test.cpp`;
- secao `[module.rtgi.0.12.0]` com os valores que o documento fixa;
- recursos em meia resolucao `R16G16B16A16_FLOAT` (960x540 em Full-HD),
  `RGB` = luz indireta, `A` = confianca;
- `shaders/rtgi.hlsl` com `PSRtgi`, que reconstroi normal e confianca e
  devolve luz indireta zerada;
- modo Insert 6 `rtgi-normals`, que desenha a reconstrucao pelo caminho novo;
- `Page Up` alternando o RTGI em tempo real, para comparacao A/B;
- `tools/shader_disasm.cpp` e `tools/shader_check.sh`.

## A ordem da pilha, e por que ela mudou de lugar

O diagrama do documento pedia:

```
SSAO -> GI -> Temporal -> Exposure/contrast/LUT -> Backbuffer
```

Mas a cadeia real era outra: `photorealism.hlsl` **e** o grading, e ele roda
primeiro. Seguir o diagrama ao pe da letra inverteria a pilha inteira e
invalidaria a calibracao consolidada - base 0.1.2 + 0.2.0 + 0.3.0, os limiares
`highlight_start/end` do SSAO e o `color_rejection` do temporal foram todos
ajustados sobre cor ja gradeada.

O motivo declarado no documento era especifico: "quero que o grading afete
tanto a luz direta quanto a indireta". Isso se resolve pondo o GI **antes** do
grading, sem mexer em mais nada:

```
scene color -> SSRTGI -> grading -> SSAO -> temporal -> backbuffer
```

O grading alcanca a luz indireta, e nenhum valor calibrado precisa ser refeito.

## Pixel shader, nao compute

Os trechos do documento estao escritos em compute shader, mas toda a pilha do
plugin sao triangulos fullscreen em `ps_5_0`, e a reconstrucao de normal ja era
uma funcao PS. Manter PS reaproveita o vertex shader, o caminho de RTV/SRV e o
tratamento de sRGB existentes, sem introduzir UAVs e dimensionamento de thread
group que nenhum passe do plugin usa hoje.

Compute passa a ser avaliado na 0.13.5, no traversal Hi-Z, que e onde ele traz
ganho real.

## Prova em bytecode

O Estagio 1 tocou tres shaders aprovados. Verificar isso no olho seria fraco, e
`fxc` nao serve: ele nao esta instalado (vem com o Windows SDK, nao com
Wine/Proton) e nao e o compilador que roda aqui. O plugin faz
`LoadLibraryW(L"d3dcompiler_47.dll")`, a ETS2 nao envia o seu proprio, e o
prefixo Proton so tem o builtin do Wine (387 KB, simbolos
`d3dcompiler_blob_vtbl`), que compila via vkd3d-shader.

`tools/shader_check.sh` compila com esse mesmo `d3dcompiler_47.dll`, os mesmos
`D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3` e o mesmo
`D3D_COMPILE_STANDARD_FILE_INCLUDE`, e desmonta o resultado.

Resultado contra a 0.11.4:

| shader | linhas de bytecode | diff |
|---|---|---|
| `photorealism.hlsl:VSMain` | 22 | 0 |
| `photorealism.hlsl:PSMain` | 271 | 0 |
| `ssao.hlsl:PSSSAO` | 1716 | 0 |
| `temporal.hlsl:PSTemporal` | 157 | 0 |
| `depth-preview.hlsl:PSDepthPreview` | 191 -> 196 | 193 |

O SSAO aprovado, com 1716 instrucoes, ficou byte-identico. O unico shader que
mudou foi o depth preview, e o histograma de opcodes mostra exatamente o que
mudou e mais nada:

```
+1 max     guarda do rsqrt: max(normal_length_squared, 1e-10)
+2 lt      comparacao que produz normal_valid
+2 movc    selecao normal_valid < 0.5 ? preto : normal
```

Antes o depth preview usava `normalize(cross(...))` sem guarda, o que produzia
NaN quando o produto vetorial degenerava. Agora a regiao invalida sai preta, do
mesmo jeito que o SSAO ja tratava.

A ferramenta fica no repositorio: toda refatoracao de shader daqui para frente
tem prova em bytecode em vez de comparacao visual.

## A guarda de atalhos

`tools/validate.sh` proibia os literais `VK_F12`, `VK_PRIOR` e `PageUp` em todo
o `src/`, sob a mensagem "atalho/captura manual proibido".

A parte do `VK_F12` era redundante. F12 e a tecla de screenshot do Steam e
continua sendo: quem garante isso e o
`SteamAPI_ISteamScreenshots_HookScreenshots`, que a propria validacao ja exige
no binario -- a captura e delegada ao Steam por API, nao conquistada pela
ausencia de um literal no codigo. A regra foi removida.

Ficaram as duas que dizem o que de fato importa:

- `steam_screenshots.cpp` nao pode conter `VK_` nem `GetAsyncKeyState`: o
  modulo de captura nunca disputa teclado com o Steam;
- `VK_PRIOR`/`PageUp` so podem existir em `postprocess.cpp`, exatamente uma
  vez, ligados ao toggle do RTGI.

## Custo

Com `enabled=false`, que e o padrao, nenhum draw novo acontece e o log so ganha
a linha de configuracao. Com `enabled=true`, um unico draw em meia resolucao
por frame, antes de qualquer escrita no backbuffer.

O buffer de GI ainda nao alimenta ninguem. Isso e proposital: compor zero e uma
operacao neutra, entao ligar o modulo nesta versao nao pode piorar a imagem.
O que a 0.12.0 mede e o custo do andaime, nao o efeito.

## Proximas fases

| versao | entrega |
|---|---|
| 0.12.1 | ray march de raio unico, hit/miss, `SkyAmbient` no miss |
| 0.13.2 | GI difusa multi-raio, `MaxIndirectLuma`, composicao com `gi_intensity` |
| 0.13.3 | acumulacao temporal com rotacao de raios e `normal_rejection` |
| 0.13.4 | denoiser bilateral depth-aware e normal-aware |
| 0.13.5 | traversal Hi-Z sobre mips de depth |
| 0.13.6 | qualidade adaptativa e presets Low/Medium/High |

A integracao com a particao de tiles do Prism3D - executar o RTGI uma unica vez
por frame, antes dos quatro draws de composicao - entra a partir da 0.13.2 e
depende da prova de composicao, hoje em investigacao na branch
`fsr-0.7.2-tiles`.
