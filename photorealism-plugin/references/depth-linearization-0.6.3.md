# Linearizacao reversed-Z 0.6.3

## Resultado da 0.6.2

O modo forward-Z ficou quase preto e o modo reversed-Z mostrou ceu branco,
objetos distantes claros e objetos proximos mais escuros. Caminhao, cerca,
vegetacao, edificios e horizonte ficaram alinhados com a imagem colorida.
Isso confirma que a textura selecionada pertence a camera principal e que a
profundidade diminui com a distancia.

## Modelo usado

Para uma projecao reversed-Z com plano distante infinito:

`distancia = near_plane / depth`

O valor zero e tratado como ceu ou distancia alem do limite. O preview divide
a distancia por `preview_distance` e satura o resultado em branco.

Os valores iniciais sao:

- `near_plane=0.1`;
- `preview_distance=200.0`.

Eles ficam em `[depth.0.6.3]`, fora das camadas de cor. `End` recarrega os
valores, o shader e a descoberta.

## Sequencia do Insert

1. normal;
2. raw;
3. reversed-Z realcado;
4. distancia linear;
5. normal novamente.

O modo linear deve produzir gradiente progressivo: proximidade escura,
distancia clara e ceu branco. Uma distribuicao excessivamente preta ou branca
indica que `near_plane` ou `preview_distance` precisa de nova calibracao antes
do SSAO.
