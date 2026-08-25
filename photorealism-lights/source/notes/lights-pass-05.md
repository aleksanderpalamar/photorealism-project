# Lights pass 05 — sinalização traseira dos veículos

## Objetivo

Melhorar discretamente a leitura das lanternas traseiras, freios e setas a
média distância sem criar círculos grandes nem iluminar artificialmente o solo.

## Alteração isolada

- `scale_factor`: `1.08x` em quatro hookups veiculares.
- Lanterna traseira: `5` passa para `5.4`.
- Freio: `5` passa para `5.4`.
- Setas esquerda e direita: `6` passa para `6.48`.

## Elementos preservados

- `default_scale`, modelos, tipos e direção.
- Cores e arquivos de lâmpadas incandescentes/LED.
- Luz projetada dos freios, setas e lanternas.
- Faróis baixo e alto, luzes de ré, posicionais dianteiras e auxiliares.
- Postes e semáforos consolidados nas versões anteriores.

## Teste recomendado

1. Seguir carros e caminhões do tráfego a curta e média distância.
2. Comparar lanterna normal e frenagem do mesmo veículo.
3. Observar setas em cruzamentos, rotatórias e mudanças de faixa.
4. Testar câmera interna e externa.
5. Repetir com chuva para observar o reflexo sem forçar o clima.
6. Confirmar FPS, frame time e ausência de halos sobrepostos.

## Critério de aceite

- Lanternas continuam discretas e vermelhas.
- Frenagem produz uma diferença visual clara sem estourar.
- Setas ficam fáceis de localizar, mantendo a cadência e o tom oficiais.
- Faróis, ré, postes e semáforos permanecem visualmente iguais à `0.4.0`.
