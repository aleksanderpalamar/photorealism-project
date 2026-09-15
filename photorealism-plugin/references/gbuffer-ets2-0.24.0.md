# G-buffer do ETS2 0.24.0

Estado: tres capturas analisadas (uma parada, duas a 60-80 km/h). A estrada esta identificada. A velocidade
do jogo e so de objetos animados; o movimento da camera precisa de outra fonte.

## A captura

`captura-quadro-20260915-115236`: ETS2 1.60, 1920x1080, FSR desligado, dia, camera na cabine em rua de
vila com o espelho a vista. 150 binds e 124 alvos gravados, sem truncar, em 1550 ms. Formatos: numero
DXGI.

Os numeros de bind e as identidades `#N` valem so para esta captura. Espelhos e faces de reflexo mudam a
contagem de passes, entao os efeitos tem que reconhecer cada alvo pela assinatura (tamanho, formatos e
numero de alvos no bind), nunca pelo indice.

## Ordem do quadro

| Binds | Alvos | O que e | Evidencia |
|---|---|---|---|
| 1-5 | depth 4096x4096 e 2048x2048, D16 | mapas de sombra (cascatas) | so depth, quadrados |
| 6-7 | #3, #4 1024x1024 R8G8B8A8_SRGB | camadas do mapa do GPS | preview: estradas e icones |
| 8-9 | #5 1920x1080 R11G11B10 + depth #6 | pre-passe de reflexo da cena perto do horizonte | preview; a confirmar |
| 10, 30 | 128x256 R11G11B10 | ceu | preview: nuvens |
| 11-22 | #9, #12 1024x1024 e #11 512x512 R11G11B10 | cubemap de reflexo externo, uma face por bind | preview: estrada, casas e ceu em faces |
| 23-29 | #13 512x512, 6 slots da mesma textura | cubemap de reflexo da cabine | preview: interior da cabine leito |
| 32-34 | 128x128 e 256x256 float, R16F e depth D16 | mapas auxiliares | desconhecido; possivelmente chuva ou umidade |
| 35-84 | 256x256 (x2) e 256x512 (x2) | quatro espelhos, cada um deferido completo: G-buffer, luz, composicao R11G11B10 (#33, #34, #43, #44) | preview: vistas invertidas da rua |
| 85-88 | #45-#47 1024x512 SRGB | telas do painel: velocimetro e GPS | preview |
| 89-90 | #48, #49 512x512 SRGB | atlas LDR dos espelhos | preview: as quatro vistas montadas; o albedo do espelho no G-buffer principal mostra essa imagem |
| 91-94 | 4 RTV 1920x1080 + depth #6 | G-buffer principal | secao abaixo |
| 95 | #54 R16_FLOAT | profundidade linear (negativa, em metros) | valores -50 a -0.6, ceu em -65500 |
| 96-98 | #55, #56 R8G8B8A8_UNORM | alternancia entre dois alvos | desconhecido; possivelmente AO do jogo |
| 99-117 | #57 rt0, #58 rt1, R16G16B16A16_FLOAT | acumulacao de luz: difusa em rt0, especular em rt1 | preview: iluminacao sem albedo; especular so em superficies brilhantes |
| 118-126 | #59 R16G16B16A16_FLOAT | composicao HDR: albedo x luz, especular, ceu, espelho, telas | preview: cena colorida antes do tom |
| 127 | #60 R8G8B8A8_SNORM | velocidade | pontos so nas folhas e no limpador; camera parada ou buffer so de objetos |
| 128-138 | 480x270, 4x2, 1x1, 240x135 | exposicao media e cadeia do bloom | tamanhos |
| 139-145 | #57, #58, #67 R8G8 (bordas binarias), #68 R16G16_UINT, #69 | anti-aliasing temporal do jogo | #67 so tem 0 e 1 nas bordas |
| 145 | #58 R16G16B16A16_FLOAT | HDR que entra no tom | maior correlacao de postos com a saida do tom (0.982, contra 0.979 do bind 144) |
| 146-147 | #70 1920x1080 R8G8B8A8_SRGB (+ depth) | saida do tom, LDR | preview |
| 148-150 | backbuffer | interface e apresentacao | |

## G-buffer principal (binds 91-94)

| Alvo | Formato | Canais | Evidencia |
|---|---|---|---|
| rt0 #50 | R16G16B16A16_FLOAT | xyz: normal (-1 a 1, comprimento 1). a: profundidade linear negativa (-68 a 0 m) | `normal_3c`; o chao sai todo da mesma cor; a alinha com o depth |
| rt1 #51 | R16G16B16A16_FLOAT | rgb: especular (cinza, canais iguais, 0-1). a: expoente de brilho (0-250, 55 valores) | preview; volante e molduras claros |
| rt2 #52 | R16G16B16A16_FLOAT | rgb: albedo linear (0-0.81). a: zero em 93%, ate 0.72 nas folhas | preview; alfa aceso so na vegetacao |
| rt3 #53 | R16G16B16A16_UINT | material, detalhado abaixo | mascaras por valor |

### Material (rt3)

- **Canal 3, byte alto: classe de sombreamento** (mascaras conferidas a olho):

  | Valor | Classe | Pixels |
  |---|---|---|
  | `0x1F` | mundo opaco: estrada, casas, cercas | 46% |
  | `0x3F` | cabine | 32% |
  | `0xBF` | pecas do caminhao: volante, molduras, espelho, painel | 9.6% |
  | `0x5F` | vegetacao: copa da arvore e arbustos | 8.1% |
  | `0` | ceu | 3.4% |

  Os bits sugerem flags: `0x20` cabine, `0x40` vegetacao, `0x80` peca do veiculo.
- **Canal 1**: 1 a 10 marcam pecas do caminhao; 32 (9.9%) cobre a superficie da estrada perto e longe, com
  vazamento na calcada e na cerca da esquerda.
- **Canal 2**: 840 em construcoes e cercas; 1 na estrada distante; 8416 na estrada perto. Ainda nao se sabe
  se a diferenca e distancia, malha ou decal.
- **Canal 0**: diferente de zero so em pecas do caminhao.

## O que isso libera

| Opcao do menu | Onde | Quando | Estado |
|---|---|---|---|
| Saturacao do albedo | rt2 do G-buffer | quando o bind de 4 RTV (f10, f10, f10, f12) em resolucao de saida e trocado | viavel |
| Pre-exposicao, pre-contraste, contraste dinamico | HDR que entra no tom | antes do bind do alvo SRGB em resolucao de saida | viavel; o alvo e conferido pelo proprio efeito |
| Espelhos do jogo | atlas 512x512 SRGB | depois dos dois binds do atlas | viavel |
| SSS | luz difusa (rt0 da acumulacao) com a mascara `0x5F` e o albedo | quando termina a acumulacao de luz | viavel |
| Normais da estrada e normais padrao | rt0 (normal) com a mascara de estrada | fim do G-buffer | mascara de estrada a confirmar |
| Motion blur | velocidade R8G8B8A8_SNORM | depois do bind da velocidade | a confirmar com o caminhao andando |
| Luz de interior | classes `0x3F` e `0xBF` | | ja funciona pelo depth; a mascara pode refinar |
| Folhas, grama, chuva do jogo | shaders do jogo | | grupo 3 (0.25.x) |

## Segunda rodada: caminhao andando

Capturas `captura-quadro-20260915-122316` (143 binds, 130 alvos) e `captura-quadro-20260915-124205` (145
binds, 128 alvos): estrada de serra e rodovia, 60-80 km/h, camera na cabine.

- **Assinaturas estaveis nas tres capturas**:
  - atlas do espelho: dois binds 512x512 SRGB;
  - G-buffer: 4 RTV f10, f10, f10, f12 em resolucao de saida;
  - velocidade: um RTV f31;
  - tom: um RTV f29 em resolucao de saida, precedido por um RTV f10 em resolucao de saida.

  Os indices mudam (G-buffer em 91, 98 e 95). Na 122316 a acumulacao de luz vem em duas partes (102-103 e
  107-109), com a alternancia RGBA8 entre elas.
- **Velocidade (f31) nao tem movimento da camera**:
  - pontos acesos so em folhas, arbustos e no limpador;
  - na estrada, a fracao de pixels nao nulos e a mesma com o caminhao parado (14%) e andando (10% e 14%);
  - o R16G16_UINT do anti-aliasing do jogo tambem nao muda na estrada entre parado e andando (mediana 14207
    contra 14203 e 15042).

  O jogo compoe o movimento da camera dentro do proprio passe temporal, a partir do depth e das matrizes.
- **Historico do anti-aliasing temporal**: e o segundo alvo do bind MRT de dois f10 (#69, #72, #71), usado
  so nesse bind. O alvo unico f10 do bind seguinte, que entra no tom, e reescrito no quadro seguinte pela
  acumulacao de luz.
- **Estrada = canal 1 do material igual a 32**, confirmado nas duas capturas andando:
  - cobre asfalto e acostamento;
  - vaza em terreno de encosta e na grama da margem;
  - somar a classe `0x1F` tira a vegetacao da margem; o terreno de rocha continua junto.
- **Canal 2 do material nao e classe**: dentro da estrada vale 8416, 7788 e 5988 conforme a cena. Parece
  parametro por objeto.

## Tabela atualizada

| Opcao do menu | Onde | Estado |
|---|---|---|
| Saturacao do albedo | rt2 do G-buffer | viavel |
| Pre-exposicao, pre-contraste, contraste dinamico | RTV f10 imediatamente antes do RTV f29 do tom | viavel |
| Espelhos do jogo | os dois atlas 512x512 SRGB | viavel |
| SSS | luz difusa, mascara `0x5F`, albedo | viavel |
| Normais da estrada e normais padrao | normal do G-buffer com `canal1 == 32` e classe `0x1F`; normal geometrica reconstruida do depth | viavel, com o terreno de rocha junto |
| Motion blur | movimento da camera fora dos buffers do jogo | pede nova fonte (proxima secao) |

## Motion blur: de onde tirar o movimento da camera

1. **Constantes do passe temporal do jogo.** No fim desse passe, as constantes que ele usou ainda estao
   ligadas; da para ler com `PSGetConstantBuffers` no mesmo gancho de `OMSetRenderTargets`, sem gancho por
   desenho. Se contiverem a matriz da camera atual e a anterior, o blur sai exato, inclusive olhando em volta
   com o mouse. A captura grava essas constantes desde a 0.24.1 (`captura-quadro-0.24.0.md`, secao
   Constantes); falta uma captura do usuario com a camera em movimento para conferir contra o depth.
2. **Telemetria oficial da SCS.** Da velocidade e rotacao do caminhao, mas nao o olhar da cabeca. Fica como
   alternativa se a opcao 1 nao achar as matrizes.
