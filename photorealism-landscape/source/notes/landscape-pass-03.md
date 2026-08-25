# Landscape pass 03 — 910–960 m

## Objetivo

Testar o alcance máximo observado no Orion Landscape e verificar se existe
ganho visual relevante sobre o checkpoint estável `0.3.0`.

## Comparação

| Perfil | Grama início/fim | Aumento sobre o jogo |
| --- | ---: | ---: |
| ETS2 1.60 oficial | 410 / 460 m | 0 m |
| Photorealism Landscape 0.3.0 | 800 / 850 m | 390 m |
| Photorealism Landscape 0.4.0 | 910 / 960 m | 500 m |

A faixa de transição permanece em 50 metros. Árvores continuam nos valores
oficiais `240–300 m`.

## Critério de decisão

- Consolidar `0.4.0` se a melhoria visual for perceptível e o desempenho
  permanecer estável.
- Preferir `0.3.0` se o ganho visual for pequeno ou houver piora no frame time.

## Teste recomendado

- Desativar a `0.3.0` e ativar somente a `0.4.0`.
- Repetir uma rota rural usada nos passes anteriores.
- Observar campos extensos, taludes e vegetação nas laterais da estrada.
- Registrar FPS, frame time, microstuttering e diferença visual percebida.

## Resultado

- FPS mantido em 60 durante o percurso.
- Frame time estável e com linha contínua.
- GPU observada aproximadamente entre 64% e 81%, sem saturação.
- Vegetação contínua em laterais, taludes e campos, sem corte LOD evidente.
- Passe aprovado e consolidado como alcance da vegetação de detalhe.
