# Depth 0.13.0 - o depth de camera era rejeitado por construcao

Data: 2026-08-28
Escopo: Photorealism 0.13.0, branch `feat/screen-space-ray-traced-global-ilumination`

## O sintoma

A 0.12.1 nunca foi validada: o RTGI jamais tracou um raio em nenhuma sessao de
teste. SSAO e resolve temporal tambem nao rodaram. Os tres modulos estavam sem
fonte, e a causa nao estava em nenhum deles.

O plugin vinha elegendo um **shadow map 2048x2048** como depth de camera.
Cascata de sombra deixa de ser vinculada quando nada projeta sombra em vista, e
ai `Depth sem atividade confirmada por 3 frames` derrubava os tres modulos
juntos -- duas vezes por sessao.

## A aritmetica

Os numeros vieram do proprio log:

```
is_scene_candidate(1920x1080, bindings=8730, elapsed=30000ms, bb=1920x1080)

  internally_scaled        = 2.073.600 x 100 >= 2.073.600 x 110
                           = 207.360.000 >= 228.096.000     ->  falso
  sustained_scene_activity = 8.730 x 1000 >= 30.000 x 400
                           = 8.730.000 >= 12.000.000        ->  falso
                                                                REJEITADO

is_scene_candidate(2048x2048, bindings=1043, ...)
  internally_scaled        = 419.430.400 >= 228.096.000     ->  verdadeiro
                                                                ACEITO
```

Duas regras produziam isso:

1. **`kMinimumScaledSceneAreaPercent = 110`** exigia que o depth tivesse 110% da
   area da tela. A regra foi escrita para achar o depth supersampleado do ETS2.
   Com `r_scale_x=1` e `r_scale_y=1` -- sem supersampling -- o depth tem
   exatamente 100% e era excluido por construcao;
2. **`kMinimumSceneBindingsPerSecond = 400`** era a valvula de escape. O depth de
   camera faz 291 binds/s (8730 em 30s, cerca de 4,85 por frame a 60fps), e nao
   alcancava.

E `aspect_is_close` existia no header desde a 0.6.x, mas so como **bonus de
pontuacao** em `static_resource_priority`, nunca como veto. Por isso um quadrado
com 43,75% de erro de proporcao podia ser eleito depth de uma tela 16:9.

## A correcao

Duas regras em `src/depth_scoring.hpp`:

**Forma passou a ser veto.** `is_plausible_scene_shape` aceita duas coisas: a
proporcao da tela, ou a tela multiplicada por um fator inteiro em cada eixo --
que e como o Prism3D faz supersampling, via `r_scale_x` e `r_scale_y`.

| alvo | proporcao | multiplo inteiro | veredito |
|---|---|---|---|
| 1920x1080 | 16:9, exata | 1x, 1x | aceito |
| 2400x1350 (ATS) | 16:9, exata | nao | aceito |
| 1920x2160 (supersampleado) | 8:9, longe | 1x, 2x | aceito |
| 2048x2048 (shadow) | erra 43,75% | nao | **vetado** |
| 4096x4096 (shadow) | erra 43,75% | nao | **vetado** |

O caso 1920x2160 e o que mostra por que a proporcao sozinha nao servia: ele e
8:9, bem longe de 16:9, e mesmo assim e o depth de cena legitimo do ETS2
supersampleado.

**Tamanho nativo passou a ser condicao suficiente.** Nova constante
`kMinimumSceneAreaPercent = 95`, separada da de 110 que continua valendo para o
caso supersampleado:

```
native_scene       = area >= 95%  da tela
internally_scaled  = area >= 110% da tela
sustained_activity = bindings/s >= 400

candidato = forma_plausivel && confiante && (native || scaled || sustained)
```

Os 95% dao folga para um depth ligeiramente menor que a tela sem abrir a porta
para meia resolucao, que e um quarto da area e cai antes, no piso de 50%.

## Uma assercao antiga que estava errada

`tests/depth_scoring_test.cpp` afirmava, desde a 0.6.x:

```cpp
assert(!is_scene_candidate(1920, 1080, 7499, 1, 1920, 1080, 30000));
```

Ou seja: um alvo do tamanho exato da tela era, por definicao, interface e nao
cena. A separacao era so a taxa de binds -- e foi exatamente essa rigidez que
excluiu o depth de camera real.

A elegibilidade nao precisa fazer essa separacao, porque **o score ja faz**:
quando o mundo supersampleado existe, `resource_score` o coloca acima, e o teste
sempre afirmou isso (`ets2_world > ets2_interface`). A assercao foi trocada pela
invariante que de fato importa.

## Telemetria

O bug sobreviveu versoes porque o log mostrava `shader_readable=nao` bem ao lado
do recurso certo, e isso parecia explicacao suficiente. Mostrava o score, nunca
o motivo da rejeicao.

`depth_candidate_rejection` devolve o primeiro motivo que barra o candidato, na
ordem em que `is_scene_candidate` avalia, e a linha `Depth recurso #N` do log
passa a imprimi-lo:

```
elegibilidade=aceito
elegibilidade=forma-incompativel
elegibilidade=bindings-insuficientes
elegibilidade=multisample
elegibilidade=menor-que-metade-da-tela
elegibilidade=area-e-atividade-insuficientes
```

Um teste garante que `elegibilidade=aceito` e `is_scene_candidate` nunca
divergem -- diagnostico que mente e pior que diagnostico ausente.

## O hook do ReShade, e por que foi descartado

A hipotese inicial era que o depth de camera do ETS2 nao era legivel por shader
(`bind_flags=0x00000040`, sem `BIND_SHADER_RESOURCE`), e que a saida seria
interceptar `CreateTexture2D` para promove-lo a typeless com o bind flag --
a tecnica que o ReShade usa.

Duas razoes para descartar:

1. **Nao resolveria.** Mesmo com o depth legivel, `is_scene_candidate`
   continuaria rejeitando o 1920x1080 e o shadow map continuaria ganhando;
2. **Nao e necessario.** O plugin **ja** copia o depth para textura propria
   `R32G8X24_TYPELESS` com `BIND_SHADER_RESOURCE` e cria o SRV sobre a copia
   (`postprocess.cpp:1670-1690`, `CopyResource` em `:530`). A fonte nunca
   precisou ser legivel; o campo `shader_readable` do log e informativo, nao e
   filtro de selecao.

Fica registrado como 0.14.0 condicional: se o `CopyResource` de um depth
`DEPTH_STENCIL`-only falhar sob DXVK, o upgrade de bind flag entra.

## O que ainda precisa ser reavaliado

Se o depth de camera nunca foi usado, o **SSAO aprovado nas versoes 0.7.0 a
0.9.1 rodou sobre uma cascata de sombra**. Os logs daquela epoca foram
sobrescritos e nao da para confirmar. Com o depth certo o efeito muda de
verdade, e `radius`, `intensity` e `fade` provavelmente vao precisar de nova
rodada de calibracao A/B. E trabalho da 0.13.7.
