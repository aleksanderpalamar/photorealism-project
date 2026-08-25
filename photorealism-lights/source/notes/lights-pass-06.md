# Lights pass 06 — flares das luzes dianteiras

## Objetivo

Dar mais presença visual ao farolete, aos faróis baixo/alto e às luzes
auxiliares/neblina vistos de frente sem aumentar o alcance ou a quantidade de
luz projetada sobre a via.

## Alteração isolada

- `scale_factor`: `1.06x` em quatro hookups.
- Posição dianteira: `2` passa para `2.12`.
- Farol baixo: `3` passa para `3.18`.
- Farol alto: `4` passa para `4.24`.
- Auxiliar/neblina: `4` passa para `4.24`.

## Elementos preservados

- `default_scale`, `visual_offset` e modelos.
- `flare_inner_angle` e direção.
- `scaling_start_distance` e `scaling_end_distance`.
- Cores, lâmpadas incandescentes/LED e iluminação projetada.
- Luzes de ré.
- Postes, semáforos e sinalização traseira consolidados anteriormente.

## Teste recomendado

1. Observar carros e caminhões vindo em sentido contrário à noite.
2. Comparar posição, farol baixo, alto e auxiliares de frente.
3. Conferir os faróis pelos retrovisores durante ultrapassagens.
4. Testar câmera interna e externa.
5. Repetir com chuva para avaliar reflexos e possível excesso de bloom.
6. Confirmar FPS, frame time e ausência de halos unidos.

## Critério de aceite

- Faróis mais presentes sem encobrir a forma do veículo.
- Diferenças entre posição, baixo, alto e auxiliares continuam claras.
- Nenhum aumento perceptível no alcance da luz projetada do caminhão.
- Demais luzes permanecem visualmente iguais à `0.5.0`.
