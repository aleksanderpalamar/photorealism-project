# Luz de interior vazando pro exterior e SSAO sem desligar - 0.24.4

Estado: entregue. Ainda nao rodou no jogo.

## Os dois relatos do usuario

1. Mexer em "Intensidade do interior" mudava tambem o brilho do que aparecia pelo
   parabrisa (asfalto, estrada) -- deveria mudar so a cabine.
2. Com "Intensidade do SSAO" no minimo, trocar entre Suave / Medio / Forte ainda
   parecia mudar a imagem, como se o SSAO nunca desligasse de verdade.

## Causa do vazamento: o limiar metro-a-metro nao batia com o carro

`cabin_weight()` (`shaders/interior_light.hlsl`) usa a distancia linearizada do
depth do jogo para decidir o quanto de cabine tem em cada pixel:
`1 - smoothstep(kInteriorLightNearStart, kInteriorLightNearEnd, distancia)`. Os
valores em uso desde a 0.23.3 eram `1.5m` a `4.0m`.

O depth principal `D32_FLOAT_S8X24_UINT` de duas capturas reais do usuario
(`captura-quadro-20260915-162158` e `-163529`), decodificado com
`tools/gbuffer_dds.py` e linearizado com `near_plane=0.1` (o mesmo valor que o
SSAO ja usa), mostra um historiograma de distancia com um vao vazio entre as duas
capturas:

| Faixa | O que e |
|---|---|
| ~0.10m a 0.12m | pico enorme (40% dos pixels do quadro): capo, painel, volante |
| 0.25m a 1.75m | **vazio nas duas capturas** -- nenhum pixel |
| a partir de ~1.75m | o resto do mundo: pista, calcada, predios, ceu |

Com o limiar antigo (1.5m-4.0m), a faixa de transicao cai quase inteira dentro do
mundo exterior, nao da cabine. Isolando so os pixels que o limiar antigo acendia e
a correcao nao, o resultado bate exatamente com o relato: **11,4% do quadro**
(235.945 pixels), todos no pavimento e na calcada visiveis por baixo do capo, nas
linhas 620 a 1079 (fundo da imagem) -- exatamente onde entra o parabrisa baixo e o
chao la fora.

**Correcao**: `kInteriorLightNearStart=0.4m`, `kInteriorLightNearEnd=1.0m`
(`src/config/effect_quality.hpp`), bem no meio do vao vazio, com folga dos dois
lados. A mesma dupla `ssao_interior_near_start`/`ssao_interior_near_end`
(`src/config/defaults.cpp`) tinha o mesmo problema com valores ainda mais largos
(2.0m-8.0m, herdados da 0.9.0) e levou a mesma correcao: e o limiar que separa o
perfil de oclusao "interior" (mais contido) do "exterior" dentro do proprio SSAO,
raio e contorno de estrada proxima nao deveriam usar o perfil da cabine.

Os numeros nao sao metros reais (o capo fica a "0.11m" nessa conta, nao a 0.11m de
verdade) porque `near_plane=0.1` e um valor de calibracao, nao o near plane real
do jogo. O que importa e o vao vazio entre os dois grupos, que apareceu igual nas
duas capturas independentes.

## Causa do SSAO nao desligar: o passe rodava do mesmo jeito

`profile_ssao_intensity=0` ja zerava a formula dentro do shader
(`visibility = saturate(1.0 - profile.intensity * normalized * fade)`, com
`profile.intensity=0` da `saturate(1.0)=1.0` em todo pixel, sem oclusao), e os
testes confirmam que a matematica bate: nenhum preset muda esse resultado quando a
intensidade e zero, porque presets so mexem em raio/vies/amostras, nunca em
intensidade.

O problema e que o passe de oclusao (`plan.ssao`) so olhava
`settings_.ssao_enabled`, um campo interno sempre `true` -- nunca checava se a
intensidade efetiva era zero. Isso deixa o resultado visual dependente so da
matematica do shader, sem a garantia dupla que a luz de interior ja tem
(`interior_light_strength(settings_) > 0.0f` no proprio `plan_frame`). Se alguma
diferenca de ponto flutuante, textura ou ordem de operacoes um dia colar um valor
minimo diferente de zero, nada no nivel do passe pegava isso.

**Correcao**: `plan.ssao` agora tambem exige `ssao_strength(settings_) > 0.0f`
(`src/postprocess/postprocessor.cpp`), pulando o passe inteiro no minimo do
slider, igual a luz de interior ja faz. Com o SSAO realmente no minimo, o passe
de oclusao nem roda -- nenhum preset tem como mudar mais nada.

## Verificacao

- `tests/interior_light_test.cpp`: `cabin_weight` no depth real do capo
  (raw=0.91) fica acima de 0,99; na distancia onde o mundo comeca nas duas
  capturas (1,75m, 2,0m, 5,9m) fica abaixo de 0,01; os limiares ficam dentro do
  vao vazio (`kInteriorLightNearEnd < 1.75`, `kInteriorLightNearStart > 0.11`).
- Reconferido nas duas capturas reais do usuario (162158 e 163529): o mesmo vao
  vazio aparece nas duas, em cenas diferentes.
- **Guardas**, quebradas numa copia: o teste da luz de interior aborta com o
  limiar antigo; a guarda do SSAO acusa quando o passe volta a rodar so por causa
  de `ssao_enabled`.

**Ainda nao rodou no jogo.** A causa do "fantasma" do SSAO em intensidade alta,
que o usuario ja tinha diagnosticado como vindo do proprio SSAO, continua aberta:
nao da pra descartar nem confirmar sem uma captura em intensidade alta, e mexer no
algoritmo de amostragem as cegas arrisca estragar o SSAO que ja estava aprovado.
