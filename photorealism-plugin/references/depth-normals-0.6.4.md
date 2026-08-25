# Normais reconstruidas 0.6.4

## Objetivo

Validar se a profundidade reversed-Z ja confirmada pode produzir geometria
local coerente antes da implementacao de SSAO. Este passe e exclusivamente
diagnostico e nao altera o tratamento de cor aprovado.

## Reconstrucao

A distancia continua usando o modelo de plano distante infinito:

`distancia = near_plane / depth`

Cada pixel e projetado aproximadamente no espaco de camera com a proporcao do
backbuffer e um FOV vertical configuravel. As posicoes dos quatro vizinhos sao
reconstruidas com o passo real da textura depth. Em cada eixo e escolhida a
derivada com menor salto de distancia, evitando misturar superficies distintas
sempre que ha uma alternativa local valida. O produto vetorial dessas derivadas
gera a normal codificada em RGB.

## Configuracao inicial

- `near_plane=0.1`;
- `preview_distance=50.0`;
- `vertical_fov=60.0`.

Esses valores ficam em `[depth.0.6.4]` e sao recarregados com `End`. O FOV e
uma aproximacao diagnostica, nao uma matriz capturada do renderizador.

## Sequencia da tecla Insert

1. normal;
2. raw;
3. reversed-Z realcado;
4. distancia linear ate 50 metros;
5. normais reconstruidas;
6. retorno ao normal.

Uma imagem de normais valida deve apresentar cores consistentes em superficies
planas, mudancas de cor conforme a orientacao e silhuetas reconheciveis. Ruido
generalizado, superficies fragmentadas ou desalinhamento indicam que a
projecao aproximada precisa de nova calibracao antes de qualquer SSAO.
