# Lights pass 02 — flares dos postes

## Objetivo

Aumentar a presença visual das lâmpadas dos postes e a resposta aparente de
bloom sem alterar a quantidade de luz projetada no ambiente.

## Alteração isolada

- Multiplicador de `scale_factor`: `1.15x`.
- 69 valores alterados em 12 arquivos de postes.
- Escalas oficiais `1` passam para `1.15`.
- Escalas oficiais `2` passam para `2.3`.

## Elementos preservados

- `ambient_color`, `diffuse_color` e `specular_color`.
- `range`, `cut_range`, `inner_angle` e `outer_angle`.
- Cores e flares dos semáforos.
- Todas as luzes dos veículos.

## Teste recomendado

1. Testar entre 22:00 e 02:00 em uma avenida urbana iluminada.
2. Observar os postes a curta, média e longa distância.
3. Passar por uma rodovia com postes e por um posto de combustível.
4. Testar com tempo seco e durante chuva.
5. Verificar se os halos continuam definidos, sem círculos gigantes ou perda
   do formato da lâmpada.
6. Confirmar FPS e frame time.

## Critério de aceite

- Flares mais presentes que na `0.1.0`, mas ainda realistas.
- Nenhuma mudança perceptível no alcance da iluminação sobre o asfalto.
- Semáforos e veículos visualmente inalterados.
