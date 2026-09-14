# EASU 0.22.4 — igual ao da AMD, bit a bit

Estado: entregue e **medido na GPU contra o codigo original da AMD**. Ainda nao
rodou dentro do jogo.

## O que estava errado ate a 0.22.3

O `fsr_easu.hlsl` era uma versao "inspirada" no EASU. Comparado linha a linha
com `ffx_fsr1.h` do FidelityFX-FSR2 v2.2.1 (clone local do usuario), divergia
em:

| Ponto | AMD | 0.22.3 |
|---|---|---|
| Forca da borda | soma horizontal + vertical | `max()` das duas |
| Luma da deteccao | `0.5·R + G + 0.5·B` | so `G` |
| Reciproco | aproximacao por bits (`0x7ef07ebb`, `0x5f347d74`) | divisao com piso `1/32768` |
| Direcao nula | `dir.x = 1`, `dir.y` mantido | `(1, 0)` |
| Forma da borda | sem `saturate` | com `saturate` |
| Normalizacao final | `aC * (1/aW)` | divisao com piso `1/32768` |
| Posicao de amostragem | `ip * con0.xy + con0.zw` | `(ip + 0.5) / out * in - 0.5` |
| Ordem de acumulacao | b c i j f e k l h g o n | b c e f g h i j k l n o |

O `max()` e o que pesa: a medida de borda chegava a no maximo 1 em vez de 2, e
depois de `* 0.5` e ao quadrado a adaptacao ficava em no maximo **25%** da AMD.
Em diagonal, onde horizontal e vertical valem as duas, a perda e a maior. O
filtro ficava largo e redondo -- borrao, e diagonais em degrau, que e o que as
capturas da 0.22.3 mostravam no GPS da cabine e na silhueta do predio.

## Como a 0.22.4 foi medida

Um programa D3D11 fora do repo (scratchpad da sessao), rodando no wine do
proprio Proton do usuario (proton-cachyos 11.0) com o DXVK dele, na RX 6600:

- compila **o `ffx_fsr1.h` original**, sem alteracao, com `FFX_GPU`,
  `FFX_HLSL`, `FFX_FSR_EASU_FLOAT`, e os callbacks `FsrEasuRF/GF/BF` feitos com
  `GatherRed/Green/Blue` como no exemplo da AMD;
- compila o `shaders/fsr_easu.hlsl` do repo;
- os dois pelo **mesmo** `d3dcompiler_47` do Proton (vkd3d-shader 2.0), que e
  o que o plugin usa no jogo;
- constantes da AMD por `ffxFsrPopulateEasuConstants` (CPU), as nossas por
  `populate_easu_constants`;
- entrada 1288x728, saida 1920x1080 em `R32G32B32A32_FLOAT`, comparacao por bit.

Duas entradas: a captura do jogo reduzida a 1288x728, e um padrao sintetico com
leque de linhas de 1 px em 26 angulos, diagonais vermelhas, xadrez, ruido e
gradiente.

| Entrada | 0.22.4 contra AMD | 0.22.3 contra AMD |
|---|---|---|
| jogo | **0 bits diferentes** em 6.220.800 canais | 4.959.684 canais diferentes, max 0,219, 1.375.698 acima de 1/255 |
| padrao | **0 bits diferentes** em 6.220.800 canais | 824.251 canais diferentes, max 0,468, 389.879 acima de 1/255 |

## Um bit que dependia do compilador

`con0[2] = 0.5 * 1288 * (1/1920) - 0.5` saiu `BE288887` no programa de teste e
`BE288888` no host. O zig nao liga FMA no alvo, mas com `-ffp-contract=on` o
LLVM **dobra a expressao constante como se fosse fundida**. No plugin as
entradas nao sao constantes e o resultado seria o estrito. Para nao depender de
flag, o produto e a subtracao ficaram em instrucoes separadas; com as flags do
plugin, e com `-ffp-contract=off` no teste, os dois lados dao `BE288888`, que e
o IEEE estrito. O `tests/fsr_easu_constants_test.cpp` prende esse valor.

## O que nao muda

- EASU e espacial: 44% dos pixels continuam 44% dos pixels. A correcao tira a
  parte do borrao e dos degraus que era do nosso shader, nao o limite do FSR 1;
- o RCAS nao foi alterado. **Correcao (0.22.6):** esta versao afirmava que ele
  ja batia com `ffx_fsr1.h`. Nao batia em quatro pontos; ver
  `fsr-rcas-lfga-0.22.6.md`;
- onde a AMD usa `rcp(x)` o shader escreve `1.0 / x`, que e a definicao.
  **Correcao (0.22.6):** esta versao afirmava que `rcp()` nao existe no
  compilador do Proton. Existe; ver `fsr-rcas-lfga-0.22.6.md`.

## Licenca

O EASU e transcricao de codigo MIT da AMD. A licenca vai em
`references/licenses/FidelityFX-FSR2-LICENSE.txt` e dentro do pacote, ao lado do
cfg.
