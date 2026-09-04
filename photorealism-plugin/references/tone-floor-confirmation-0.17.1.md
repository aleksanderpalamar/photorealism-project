# Confirmacao em jogo da 0.17.1

Doze capturas no ETS2, aprovadas pelo usuario. Medidas com o mesmo script
rodado nas cinco referencias do ATS na mesma sessao, lado a lado.

## O pipeline mudou entre as duas versoes

A 0.17.0 foi capturada em 1080p nativo. A 0.17.1 foi capturada com

    gamescope -W 1920 -H 1080 -w 2560 -h 1440 -F fsr --sharpness 4 -f

No gamescope `-w/-h` e a resolucao do jogo e `-W/-H` a da saida: o jogo
renderiza em 2560x1440 e entrega 1920x1080. E supersampling, nao upscaling,
e `--sharpness` vai de 0 (max) a 20 (min), entao 4 e quase o maximo de
nitidez. Isso obriga a separar o que e a correcao do que e o pipeline.

## O que a correcao entregou

| medida                  | referencias (5)     | 0.17.1 (12)          | 0.17.0        |
| ----------------------- | ------------------- | -------------------- | ------------- |
| % >= 250 nos tres canais | 0,0000 a 0,0003     | 0,0000 nas doze      | 0,0000        |
| piso R / G / B          | 2,7-7,5 / 4,3-7,8 / 6,5-11,2 | 3,7-4,3 / 6,5-7,3 / 7,5-8,4 | 8/8/8 a 9/9/9 |
| R=G=B exato na sombra   | 0,00 a 0,90%        | 0,00 a 0,15%         | 71,8 a 89,9%  |
| chroma no meio-tom      | 0,161 a 0,237       | 0,183 a 0,244        | -             |

O piso caiu dentro da faixa nos tres canais. No `23-02-47`, com o sol
visivel no horizonte, nada chega a 255 e nenhum pixel tem os tres canais
em 250: o ombro segurou o caso mais dificil do lote.

A saturacao nao abriu, que era o risco anotado do contraste em potencia e
nao dava para prever por medicao. As tres capturas de chroma alto (0,322,
0,350, 0,365) sao luz dourada rasante e vapor de sodio no conves do ferry.
`saturation` fica em 0,97.

## O supersampling nao explica o resultado

Um downsample 4:3 e uma media local: ele quebra plato na borda das
regioes, nunca no interior. Emulando o crush da 0.17.0 numa referencia e
reamostrando 4:3 como o gamescope faz:

| estado             | niveis abaixo de 12/255 | R=G=B exato   |
| ------------------ | ----------------------- | ------------- |
| esmagado           | 1 / 1 / 1               | 100%          |
| esmagado + 4:3     | 12-15 / 6 / 12-13       | 97,1 a 99,0%  |

Se a 0.17.1 ainda tivesse o crush, o supersampling teria entregue ~98% de
cinza exato. Mediu 0,00 a 0,15%.

O mesmo experimento invalida a contagem de niveis distintos como prova de
estrutura: um plato reamostrado produz 12/6/13 niveis, a mesma ordem de
grandeza que a 0.17.0 media em 1080p nativo. E o segundo caso, depois do
p1 do grade_report.py, de uma metrica que passa na imagem quebrada.

## Falta grao na sombra

Ruido de alta frequencia (sinal menos media 5x5, nos 30 blocos mais lisos
por imagem), com a autocorrelacao do residuo entre vizinhos:

| faixa    | conjunto | std   | r(+1,x) | r(+1,y) |
| -------- | -------- | ----- | ------- | ------- |
| sombra   | REF      | 1,687 | -0,094  | -0,137  |
| 4 a 20   | 0.17.1   | 0,178 | -0,033  | +0,016  |
| meio-tom | REF      | 1,532 | -0,020  | -0,116  |
| 80 a 180 | 0.17.1   | 2,090 | +0,131  | +0,055  |

O downsample atenua ruido por raiz de 1,778 = 1,33x, e o RCAS em sharpness
4 amplificaria qualquer grao presente. A lacuna corrigida ainda e ~7x.

O decisivo e a correlacao: no meio-tom o residuo da 0.17.1 e positivamente
correlacionado, a assinatura da reamostragem, porque pixels de saida
vizinhos dividem pixels de origem. Na sombra da mesma imagem essa
correlacao nao existe. Um filtro de reamostragem e uniforme e nao consegue
apagar ruido na sombra deixando estrutura correlacionada no meio-tom.

Consequencia para o dither: ele nao precisa ser global, o meio-tom ja esta
na magnitude da referencia. E a amplitude tem que ser calibrada contra o
caminho real de saida, porque o passe roda a 2560x1440 e perde 1,33x no
downsample antes de chegar na tela.

## O ajuste que sobrou

Filtrando com mediana 3x3 para remover o undershoot do RCAS, que enviesa a
ancora de piso para baixo em 0,4 a 0,9 codigos:

| piso     | R    | G    | B    | B/G   |
| -------- | ---- | ---- | ---- | ----- |
| cru      | 3,96 | 6,89 | 7,97 | 1,156 |
| mediana  | 4,52 | 7,77 | 8,40 | 1,082 |

As referencias tem B/G de 1,372 a 1,538 e R/G de 0,620 a 0,952. A sombra
da 0.17.1 esta menos azul e um pouco menos vermelha que o alvo. E o que a
0.17.2 vai mirar.

## O que nao da para afirmar

As capturas da 0.17.0 foram apagadas, entao o antes/depois usa os numeros
da medicao anterior, com mascara de sombra ligeiramente diferente. A
direcao e inequivoca; o par exato nao foi re-medido no mesmo script.
