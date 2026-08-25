# Landscape pass 04 — árvores 330–390 m

## Objetivo

Reduzir o aparecimento repentino das árvores com um primeiro aumento
conservador, mantendo a vegetação de detalhe consolidada em `910–960 m`.

## Comparação

| Perfil | Árvores início/fim | Grama início/fim |
| --- | ---: | ---: |
| ETS2 1.60 oficial | 240 / 300 m | 410 / 460 m |
| Photorealism Landscape 0.4.0 | 240 / 300 m | 910 / 960 m |
| Photorealism Landscape 0.5.0 | 330 / 390 m | 910 / 960 m |

A faixa de transição das árvores permanece em 60 metros. O aumento é de 90
metros sobre o jogo-base.

## Teste recomendado

- Desativar a `0.4.0` e ativar somente a `0.5.0`.
- Percorrer regiões florestais, curvas com árvores e entradas de cidades.
- Observar o surgimento de árvores e mudanças bruscas entre modelos LOD.
- Registrar FPS, GPU, frame time e qualquer microstuttering.
- Conferir se a paisagem distante fica mais contínua sem vegetação excessiva.

## Resultado

- FPS mantido em 60, com mínimo observado de 59.
- Frame time estável.
- GPU observada aproximadamente entre 58% e 77%.
- CPU observada entre 10% e 12%, sem gargalo.
- Linhas de árvores contínuas nas laterais e no horizonte.
- Passe aprovado como primeiro checkpoint das árvores.
