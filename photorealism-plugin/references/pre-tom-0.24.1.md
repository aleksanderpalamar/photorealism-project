# Pre-tom 0.24.1

Estado: entregue. Ainda nao rodou no jogo. Primeiro efeito entre passes do grupo 2.

## O que faz

Pre-exposicao, pre-contraste e contraste dinamico do conjunto de tom ativo passam a agir no HDR do jogo,
antes do tom do proprio jogo. Ate a 0.24.0 eram lidos do cfg e nao mudavam nada.

## Onde

- **Alvo**: o RTV unico `R16G16B16A16_FLOAT` (view 10) em resolucao de saida que vem logo antes do RTV
  unico `R8G8B8A8_UNORM_SRGB` (view 29) do mesmo tamanho. E a assinatura do tom nas tres capturas de
  `gbuffer-ets2-0.24.0.md` (binds 145-146, 137-138 e 140-141).
- **Reconhecimento**: pela forma dos dois binds seguidos, nunca pelo indice. O segundo bind do tom (o
  mesmo SRGB com depth) nao dispara, porque o bind anterior a ele ja nao e HDR.
- **Momento**: no gancho de `OMSetRenderTargets` do jogo que liga o alvo do tom, depois da captura de
  quadro e antes de o jogo desenhar o tom. Nenhum gancho por desenho.

## Nao volta no quadro seguinte

O alvo do pre-tom nao e historico de nada:

| Captura | HDR do tom | Usos no quadro | Historico do anti-aliasing |
|---|---|---|---|
| 115236 | #58 | luz 99-117, 142, 145 | #69, so no bind MRT 144 |
| 122316 | #59 | luz 102-109, 134, 137 | #72, so no bind MRT 136 |
| 124205 | #60 | luz 102-110, 137, 140 | #71, so no bind MRT 139 |

O quadro seguinte reescreve esse alvo na acumulacao de luz. O historico do anti-aliasing temporal fica no
segundo alvo do bind MRT, que o pre-tom nao toca. A medicao de exposicao do jogo (480x270 ate 1x1) roda
antes, sobre a composicao, entao o ganho nao e compensado pela exposicao automatica.

## Formula

O cfg de referencia nao documenta a formula; esta e a leitura adotada:

- ganho = `2^pre_exposure` (EV);
- contraste = `pre_contrast * dynamic_contrast`;
- na luminancia (Rec.709) do HDR ja com o ganho: `0.18 * (luma / 0.18)^max(1 + contraste, 0.1)`;
- a cor e escalada pela razao entre a luminancia nova e a antiga, o que preserva o matiz;
- teto 60000, alfa intacto.

Contraste positivo afasta do cinza medio: claros sobem, escuros descem, 0.18 fica parado.

| Iluminacao | pre_exposure | pre_contrast | dynamic_contrast | Ganho | Contraste |
|---|---|---|---|---|---|
| A (conjunto 1) | -1.00 | 0.42 | 1.00 | 0.500 | 0.42 |
| B (conjunto 2) | 0.00 | 0.26 | 1.00 | 1.000 | 0.26 |
| C (conjunto 3) | 0.10 | 0.00 | 1.00 | 1.072 | 0 |
| D (conjunto 4) | 0 | 0 | 1 | 1.000 | 0 |

Com ganho 1 e contraste 0 o passe nao roda: a iluminacao D padrao nao muda.

## Mecanica

- `src/passfx/tone_stage.hpp`: reconhece o par HDR -> tom.
- `src/passfx/pre_tone_effect.cpp`: copia o HDR para uma textura propria, desenha um triangulo de tela cheia
  com `shaders/pre_tone.hlsl` no RTV do jogo e devolve o estado com `capture_state`/`restore_state`, mais
  os 128 slots de textura do pixel shader.
- `src/passfx/pass_effects.cpp`: junta as duas coisas e escreve o log.
- O processador liga um sinal atomico no Present quando o pre-tom tem trabalho; sem ele, o gancho sai sem
  travar nada.

## Log

- `Pre-tom 0.24.1 pronto` na compilacao do shader.
- `Pre-tom 0.24.1 ativo: ganho=... (... EV) contraste=... no HDR 1920x1080 antes do tom do jogo.` a cada
  troca de valores.
- A cada 10 s: `Pre-tom 0.24.1: N quadros aplicados nos ultimos 10 s.`, ou o aviso de que o passe do tom
  nao apareceu.

## Verificacao fora do jogo

Harness Wine + DXVK com um "jogo" sintetico que liga um HDR, desenha, liga o alvo SRGB e amostra o HDR no
slot 5 com um constant buffer no slot 3:

- ganho 0.5 e contraste 0.42 sobre (0.4, 0.2, 0.1, 0.7): esperado 0.1673 / 0.0836 / 0.0418 / 0.7, lido
  0.1671 / 0.0836 / 0.0418 / 0.6997;
- depois do passe, o alvo ligado e o do tom, a textura do slot 5 e o buffer do slot 3 sao os do jogo;
- um segundo bind do tom sem HDR antes nao mudou o alvo.
