# RCAS e LFGA 0.22.6 — iguais aos da AMD, bit a bit

Estado: entregue e **medido na GPU contra o codigo original da AMD**. Ainda nao
rodou dentro do jogo.

## A correcao que abriu esta versao

A 0.22.4 afirmou que o RCAS "ja batia" com `ffx_fsr1.h`. Nao batia:

| Ponto | AMD | ate a 0.22.5 |
|---|---|---|
| Reducao de ruido | opcional, desligada (`FSR_RCAS_DENOISE`) | sempre ligada |
| Divisao final | `ffxApproximateReciprocalMedium` | divisao exata |
| Limitadores | `rcp()` sem piso | divisao com piso `1/32768` |
| Nitidez | stops, `exp2(-stops)`; a API do FSR 2 remapeia `stops = 2 - 2 * nitidez` | multiplicador linear |

A 0.22.4 tambem afirmou que `rcp()` nao existe no compilador HLSL do Proton. Um
shader de teste com `rcp(4.0)` **compila** nele, e o proprio EASU de referencia
da AMD, que usa `rcp`, tinha compilado na mesma sessao. A checagem que embasou a
afirmacao foi procurar a string na DLL -- ausencia de string nao prova ausencia
de intrinseco. A guarda saiu.

## O que mudou

- `fsr_rcas.hlsl` transcreve `FsrRcasF` sem `FSR_RCAS_DENOISE` e sem
  `FSR_RCAS_PASSTHROUGH_ALPHA`. O calculo de ruido, que a AMD so usa com o
  define ligado, nao entra: e codigo morto no caminho padrao;
- a leitura de vizinho (`FsrRcasLoadF`, callback da aplicacao) prende a borda;
  o exemplo da AMD usa `Load` puro, cujo resultado fora da textura e indefinido
  no Vulkan;
- a nitidez segue `FsrRcasCon` com o remapeamento da API do FSR 2
  (`ffx_fsr2.cpp`: `(-2 * sharpness) + 2`). 0.60 vira 0.8 stop, multiplicador
  0,5743 -- perto do 0,60 linear que o usuario tinha escolhido;
- LFGA: `FsrLfgaF` como esta, aplicado depois do RCAS, em linear, com a
  saida recodificada para o RTV. Granulacao monocromatica, como o exemplo da
  AMD.

## A granulacao

O `ffx_fsr1.h` pede granulacao na resolucao de saida, em linear, variando no
tempo com soma temporal zero por pixel, e sugere ruido azul. A implementacao:

- ruido azul 64x64 gerado por void-and-cluster (sigma 1,9, toroidal) na
  criacao do pipeline, em `R32_FLOAT`; os 4096 valores sao uma permutacao de
  `(k + 0,5) / 4096`;
- animacao: `frac(ruido + quadro * 0,618...) - 0,5`, a sequencia da razao aurea,
  que distribui cada pixel uniformemente no tempo;
- contra ruido branco da mesma variancia, a variancia das medias locais cai
  para 47% (2x2), 22% (4x4) e 13% (8x8): falta baixa frequencia, que e o que
  define ruido azul.

## Como foi medido

Mesmo metodo da 0.22.4: programa D3D11 fora do repo, wine e DXVK do Proton do
usuario, RX 6600, o `ffx_fsr1.h` original compilado pelo mesmo `d3dcompiler_47`
que o plugin usa.

| Teste | Resultado |
|---|---|
| RCAS, nitidez 0.00 / 0.35 / 0.60 / 1.00, tres entradas | 0 bits diferentes em 6.220.800 canais cada |
| constantes `FsrRcasCon` contra as nossas | bits iguais (`3E800000`, `3ECFEFC6`, `3F13088D`, `3F800000`) |
| LFGA, quantidade 0.37, tres entradas | 0 bits diferentes |
| `rcp()` no compilador do Proton | compila |
| PS completo sem granulacao, RTV sRGB e UNORM | a no maximo 1 byte do RCAS |
| PS com granulacao 0.15, 16 quadros | 1,16 byte de desvio medio por quadro, vies de -0,003 byte |

As entradas: a saida do EASU da AMD sobre a captura do jogo, sobre o padrao de
linhas finas, e um padrao de extremos com xadrez 0/1 e areas pretas e brancas
puras -- o caso em que os limitadores dividem por zero.

## Licenca

Codigo MIT da AMD; a licenca continua em
`references/licenses/FidelityFX-FSR2-LICENSE.txt` e no pacote.
