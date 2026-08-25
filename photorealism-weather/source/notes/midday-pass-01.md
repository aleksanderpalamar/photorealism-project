# Calibração de meio-dia — passe 01

Escopo: `default.nice.14` até `default.nice.22`.

Objetivo: estabelecer uma gradação neutra para comparação dentro do jogo, sem
trocar os skyboxes originais e sem alterar exposição automática global.

## Ajustes

| Campo | Multiplicador | Intenção |
|---|---:|---|
| `ambient` | 0.97 | Reduzir levemente o aspecto plano. |
| `sun_shadow_strength` | 0.92 | Evitar sombras excessivamente pesadas. |
| `fog_density` | 1.08 | Reforçar discretamente a perspectiva atmosférica. |
| `color_saturation` | 0.96 | Conter cores artificiais sem desbotar a cena. |
| `contrast` | 0.95 | Preservar mais detalhe nos extremos tonais. |
| `bloom_threshold` | 1.25 | Restringir bloom a fontes mais intensas. |
| `bloom_limit` | 0.90 | Limitar o pico do bloom. |
| `bloom_intensity` | 0.60 | Remover o aspecto enevoado do pós-processamento. |
| `sunshaft_color` | 0.70 | Tornar raios solares menos artificiais. |

`target_gray`, `diffuse`, `specular`, cores do céu e texturas permanecem originais
neste passe. Os resultados precisam ser avaliados no jogo antes de alterar energia
da luz ou exposição.

## Critérios de comparação

- Céu sem estouro excessivo.
- Detalhes presentes em fachadas claras e vegetação escura.
- Sombras com profundidade, mas sem preto fechado.
- Bloom pouco perceptível em superfícies comuns.
- Neblina visível principalmente em média e longa distância.

