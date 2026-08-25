# Lights pass 07 — projeção dos faróis

## Objetivo

Melhorar a leitura noturna da via com um ganho realista e controlado no alcance
e na energia projetada pelos faróis baixo e alto, sem transformar a estrada em
uma área uniformemente clara.

## Cobertura

- 47 perfis oficiais do ETS2 1.60.
- Caminhões clássicos e modernos, incluindo modelos distribuídos nos DLCs
  oficiais de caminhões.
- Tecnologias halógena, xenon e LED preservadas individualmente.

## Alterações

- `low_beam_range`: `1.05x`.
- `hi_beam_range`: `1.05x`.
- `low_beam_color` e `low_beam_color_specular`: `1.03x` nos três componentes.
- `hi_beam_color` e `hi_beam_color_specular`: `1.03x` nos três componentes.

## Elementos preservados

- Proporção RGB e, portanto, a assinatura cromática de cada farol.
- Cores ambientes, ângulos, aspecto, inclinação e rotação.
- Máscaras de recorte e atenuação.
- Fachos refratados e suas frações.
- Fachos auxiliares dianteiros e de teto.
- Flares consolidados na `0.6.0` e todas as demais famílias de luz.

## Teste recomendado

1. Estrada rural sem postes, entre 00:00 e 03:00.
2. Túnel ou parede plana para comparar recorte do baixo e preenchimento do alto.
3. Farol baixo em reta, curva e aclive, observando o limite iluminado.
4. Farol alto em reta longa, comparando distância e placas refletivas.
5. Chuva natural para avaliar o componente especular sem forçar o clima.
6. Câmera interna e externa, acompanhando FPS e frame time.

## Critério de aceite

- Ganho perceptível de alcance sem iluminar o horizonte inteiro.
- Centro do facho mais legível sem estourar as faixas ou paredes próximas.
- Recorte do farol baixo permanece definido.
- Farol alto continua claramente mais longo que o baixo.
- Temperaturas halógena, xenon e LED continuam visualmente distintas.
