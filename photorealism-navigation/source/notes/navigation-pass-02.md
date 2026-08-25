# Navigation pass 02 — Light e Dark

## Objetivo

Validar a primeira identidade visual do GPS, inspirada na clareza dos mapas
modernos e mantendo uma identidade própria da coleção Photorealism.

## Arquitetura

O ETS2 1.60 possui uma única paleta global no `map_data.sii`. O escurecimento
noturno do HUD não troca individualmente as cores do mapa. Por isso, este passe
gera duas variantes selecionáveis e mutuamente exclusivas:

- `Photorealism Navigation - Light`
- `Photorealism Navigation - Dark`

## Cobertura visual

- Route Advisor padrão.
- GPS comum usado por diversos caminhões e acessórios.
- Volvo FH 2021 e FH 2024, em km/h e mph.
- Mapa mundial e mapas de seleção de trabalho por meio do `map_data.sii`.

## Roteiro de teste

1. Deixar apenas a variante `Light` ativa.
2. Testar o Route Advisor e o GPS da cabine entre 10:00 e 14:00.
3. Abrir o mapa mundial e observar estradas, áreas de empresas e rota.
4. Substituir `Light` por `Dark`; nunca manter as duas ativas.
5. Testar entre 22:00 e 02:00 e verificar se a rota continua legível sem
   iluminar excessivamente a cabine.
6. Confirmar ausência de erros de `map_data.sii`, `adviser_gps.sii` e scripts
   Volvo no `game.log.txt`.

## Pontos para observar

- Contraste da rota azul sobre estradas descobertas.
- Legibilidade das setas de cruzamento.
- Brilho do fundo Light dentro da cabine durante o dia.
- Conforto do fundo Dark à noite.
- Compatibilidade visual com o Photorealism e o Photorealism Landscape.
