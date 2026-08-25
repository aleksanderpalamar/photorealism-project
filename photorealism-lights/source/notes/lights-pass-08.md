# Lights pass 08 — projeção das luzes auxiliares

## Objetivo

Melhorar de forma controlada a contribuição das luzes auxiliares dianteiras,
de neblina e de teto, preservando a hierarquia dos faróis e evitando um excesso
de luz no primeiro plano.

## Cobertura

- 47 perfis oficiais do ETS2 1.60.
- Tecnologias halógena, xenon e LED preservadas individualmente.
- Estados auxiliar dianteiro, auxiliar de teto e dianteiro+teto.

## Alterações

- `front_beam_range`: `1.03x`.
- `front_beam_color` e `front_beam_color_specular`: `1.02x`.
- `roof_beam_range`: `1.04x`.
- `roof_beam_color` e `roof_beam_color_specular`: `1.03x`.
- `front_roof_beam_range`: `1.04x`.
- `front_roof_beam_color` e `front_roof_beam_color_specular`: `1.03x`.

## Elementos preservados

- Proporção RGB e assinatura cromática de cada tecnologia.
- Ângulos, aspecto, inclinação e rotação.
- Máscaras de distribuição e atenuação.
- Fachos refratados e cores ambientes.
- Projeção consolidada dos faróis baixo e alto.
- Flares auxiliares consolidados na `0.6.0`.

## Teste recomendado

1. Estrada rural sem postes, entre 00:00 e 03:00.
2. Começar apenas com farol baixo e registrar o alcance visual.
3. Ativar as auxiliares dianteiras/neblina e observar o preenchimento próximo e
   intermediário, especialmente nas laterais da via.
4. Desativar as dianteiras, ativar apenas as auxiliares de teto e observar o
   ganho de distância.
5. Ativar dianteiras e teto juntas, verificando se o centro do facho continua
   com textura e sem mancha branca.
6. Repetir em parede, curva, aclive e chuva natural, acompanhando FPS e frame
   time.

## Critério de aceite

- Dianteiras/neblina melhoram o preenchimento sem superar o farol alto.
- Auxiliares de teto alcançam mais longe que as dianteiras.
- Estado combinado permanece forte, mas não estoura faixas, placas ou paredes.
- Temperaturas halógena, xenon e LED continuam distintas.
- Nenhum halo excessivo ou perda de detalhe no asfalto próximo à cabine.
