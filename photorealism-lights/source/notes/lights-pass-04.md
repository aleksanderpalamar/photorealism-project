# Lights pass 04 — flares dos semáforos

## Objetivo

Melhorar discretamente a leitura dos semáforos durante a noite sem transformar
as lâmpadas em halos grandes nem antecipar artificialmente seu aparecimento.

## Alteração isolada

- `scale_factor`: `1.10x` nos 71 flares de semáforos.
- Escala oficial `2` passa para `2.2`.
- Sinais comuns, direcionais, de bonde, pedágio e variantes regionais cobertos.

## Elementos preservados

- `traffic_light_lamp_colors.sii` completo.
- `ambient_color`, `diffuse_color` e `specular_color`.
- `range`, `cut_range`, ângulos e direção.
- `fade_distance` e `fade_span`.
- Estados, sequências, temporização e modelos.
- Toda a calibração consolidada dos postes.
- Todas as luzes dos veículos.

## Teste recomendado

1. Testar entre 22:00 e 02:00 em cruzamentos urbanos.
2. Observar vermelho, amarelo e verde a curta, média e longa distância.
3. Conferir sinais direcionais e semáforos vistos em ângulo lateral.
4. Fazer uma passagem com chuva para verificar possível excesso de bloom.
5. Testar câmera interna e externa.
6. Confirmar FPS, frame time e ausência de halos que unam lâmpadas próximas.

## Critério de aceite

- Cores mais fáceis de identificar, especialmente à média distância.
- Contorno e símbolo direcional continuam legíveis.
- Nenhum aparecimento antecipado em relação à versão anterior.
- Postes e luzes de veículos permanecem visualmente iguais à `0.3.0`.
