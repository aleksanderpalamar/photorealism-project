# Landscape pass 01 — 650–700 m

## Objetivo

Reduzir o aparecimento repentino da vegetação de detalhe sem introduzir perda
excessiva de FPS ou instabilidade no tempo de quadro.

## Valores conhecidos

| Perfil | Árvores início/fim | Grama início/fim |
| --- | ---: | ---: |
| ETS2 1.60 oficial | 240 / 300 m | 410 / 460 m |
| Photorealism Landscape 0.2.0 | 240 / 300 m | 650 / 700 m |
| Orion Landscape 1.2 | 240 / 300 m | 910 / 960 m |

O Orion preserva as árvores e acrescenta 500 metros ao início e ao fim do LOD
da grama. A faixa de transição continua com 50 metros.

## Estratégia sugerida

Testar aumentos graduais em uma rota fixa, começando abaixo do valor do Orion.
Para cada passe, registrar:

- média de FPS e pior queda observada;
- estabilidade do tempo de quadro;
- distância em que a grama aparece;
- comportamento em áreas rurais densas e entradas de cidades;
- configurações de densidade e distância da vegetação usadas no jogo.

## Primeiro candidato

A baseline `0.1.0` carregou e ativou normalmente no jogo. O primeiro candidato
usa `650–700 m`, acrescentando 240 metros ao alcance oficial e preservando os
50 metros de transição. Isso representa menos da metade do aumento total usado
pelo Orion.

Comparar `0.1.0` e `0.2.0` preferencialmente no mesmo trecho rural, horário e
clima. Observar principalmente as laterais da pista, campos abertos e taludes.

## Resultado

- Vegetação percebida como mais nítida e contínua.
- FPS mantido em 60.
- Linha de frame time estável.
- Passe aprovado como checkpoint de desempenho.
