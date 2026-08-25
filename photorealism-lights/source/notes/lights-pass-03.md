# Lights pass 03 — projeção dos postes

## Objetivo

Melhorar discretamente a continuidade da iluminação sobre o asfalto sem criar
áreas excessivamente claras ou aumentar demais o volume calculado pela GPU.

## Alterações

- Flares preservados em `1.15x`.
- `range` e `cut_range`: `1.05x` em 61 fontes.
- Primeiro componente de `diffuse_color`: `1.06x` em 61 fontes.
- Primeiro componente de `specular_color`: `1.06x` em 61 fontes.

## Elementos preservados

- `ambient_color` completo.
- Matiz e saturação de `diffuse_color` e `specular_color`.
- `inner_angle`, `outer_angle`, direção e modelos.
- Semáforos e todas as luzes dos veículos.

## Teste recomendado

1. Avenida urbana com postes próximos e sobreposição entre fontes.
2. Rodovia iluminada com postes mais espaçados.
3. Posto de combustível ou pátio industrial.
4. Tempo seco e chuva para observar o componente especular.
5. Câmera interna e externa.
6. Confirmar FPS, frame time e ausência de manchas ou faixas luminosas.

## Critério de aceite

- Menos interrupção escura entre postes consecutivos.
- Asfalto um pouco mais legível sem parecer iluminado por holofotes.
- Reflexo úmido mais presente, porém sem estourar.
- Flares continuam iguais à `0.2.0`.
