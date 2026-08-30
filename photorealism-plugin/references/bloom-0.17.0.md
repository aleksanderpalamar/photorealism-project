# Bloom 0.17.0 — a estrutura, com a calibracao pendente

Estado: **estrutura entregue, parametros NAO medidos.** Os quatro numeros do
cfg sao derivacao fisica; a medicao contra as referencias do ATS e a 0.17.1 e
depende das imagens.

## O que o plugin nao conseguia fazer

Todo efeito de `photorealism.hlsl` e por pixel: pega um valor e devolve outro
valor no mesmo lugar. A unica excecao olha quatro vizinhos a um texel. Bloom e
energia atravessando dezenas de pixels.

E o mesmo tipo de limite que a 0.14.0 encontrou em `apply_temperature`, que so
trocava R contra B: nenhuma combinacao dos parametros existentes alcancava o
alvo, porque **faltava o eixo**. Aqui faltava o passe.

## Por que uma piramide, e nao um blur maior

Um separavel de nove taps a meia resolucao alcanca cerca de 8 pixels de sigma
-- 0,007 da altura em 1080p. O flare de sol das referencias esta na casa dos
centesimos da altura, uma ordem de grandeza acima.

Chegar la esticando os offsets do kernel deixa buracos entre as amostras e o
glow vira anel em vez de queda suave. Cada nivel da piramide dobra o alcance
pelo mesmo custo, porque o numero de pixels cai pela metade junto.

**Por isso o alcance e escolhido pela contagem de niveis**, e `radius` no cfg e
uma fracao da altura da tela que vira `level_count` no C++:

```
L = log2(radius * altura / 1.5)
```

Isso e o que faz o mesmo `0.04` valer em 1080p e em 4K.

## A decisao que evitou multiplicar ramos

A cadeia do frame ja e um `if/else` de cinco ramos. Um passe de composicao
separado teria multiplicado isso por dois -- e foi por multiplicar ramos que o
modulo de tracado de raios chegou a 377 referencias num arquivo so.

Mas os cinco ramos **comecam todos pelo mesmo passe visual**. Entao a piramide
roda antes do `if`, o `PSMain` le o resultado em `t1` e soma. Bloom vale para
os cinco ramos de uma vez, com **zero ramos novos**.

## Onde a soma entra, e por que ali

Entre o sharpening e `apply_tonal_controls`. As tres fronteiras importam:

- **depois do sharpening**, senao o realce morde a borda do glow e devolve halo
  duplo -- realcar uma coisa que e suave por natureza;
- **antes dos controles tonais**, para o brilho receber exposicao, temperatura
  e tint junto com a cena. O flare do sol na golden hour tem que sair QUENTE
  porque a imagem inteira e quente, e nao cinza colado por cima;
- **e portanto antes de `apply_highlight_rolloff`**, que comprime a soma. O
  ombro da 0.14.0 e o que torna somar luz aqui seguro; sem ele isto seria um
  plato branco.

`validate.sh` guarda essa faixa por numero de linha, no molde da guarda do
`black_lift`.

## O zero exato

A propriedade central do limiar e que a contribuicao e **exatamente zero**
abaixo do joelho, e nao apenas pequena. Se um pixel escuro contribui qualquer
coisa, todo pixel da cena contribui, o blur espalha isso pela tela inteira e o
bloom deixa de ser brilho em volta de fontes para virar nevoa cinza uniforme --
o artefato que faz um bloom parecer amador.

`tests/bloom_curve_test.cpp` prova isso em cem pontos abaixo do joelho.

O joelho tambem e o que impede o glow de PISCAR: com corte reto, um farol que
oscila em volta do limiar entraria e sairia inteiro a cada frame.

## Pendente: a medicao

`tools/bloom_report.py` mede o perfil radial em volta das fontes e devolve
exatamente os tres parametros. Conferido contra alvos sinteticos de raio
conhecido: recupera 0,035 como 0,038 e 0,094 como 0,100 -- vies sistematico de
cerca de 7% para cima, porque o primeiro anel media um disco e nao um ponto.

Enquanto os numeros nao forem medidos, o cfg carrega o aviso e `validate.sh`
guarda o aviso. Se alguem apagar o aviso sem medir, o proximo a ler o arquivo
acredita nos numeros.
