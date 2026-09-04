# Bloom 0.17.0 — e a medicao que corrigiu a premissa

Estado: entregue e **medido**. A medicao contradiz o modulo, e ele fica ligado
mesmo assim por decisao do usuario, com intensidade baixa e a ressalva escrita
no cfg.

## A medicao, e o que ela derrubou

As cinco referencias do ATS foram medidas depois que a estrutura ficou pronta.
**Elas nao tem bloom.**

- toda borda entre muito claro e muito escuro esta **nitida**: o quadro do
  para-brisa contra o ceu, as nuvens brancas contra a cabine, o capo branco
  contra o painel;
- o perfil de borda em resolucao cheia mostra o lado escuro **plano em 22-24
  ate a transicao, sem cauda** -- e cauda no lado escuro e exatamente a
  assinatura que bloom deixaria;
- na noturna, os mostradores acesos **nao iluminam nada em volta**;
- `topo%` e 0,00 nas cinco: nada e estourado, que e o regime em que bloom mais
  aparece.

**A afirmacao de que as referencias mostravam "halacao na borda do para-brisa"
estava errada.** Ela foi observacao a olho, nunca conferida nos pixels, e virou
metade da justificativa desta versao.

O que as referencias tem sao **raios de sol**: estriados radiais saindo do sol
atras da linha de arvores, projetados no teto escuro da cabine. Sao
**direcionais**, e uma piramide gaussiana nao produz aquilo -- ela faz halo
redondo e simetrico. Isso e a 0.18.0.

Dos quatro parametros, **so o limiar e medido**: 0.85 em sRGB e o codigo 217,
acima do p95 das cinco referencias (117 a 212), o que faz o bloom pegar o disco
do sol, o topo das nuvens e o realce do capo em vez do ceu. Os outros tres sao
escolha, e `intensity` fica baixo de proposito.

## Confirmado em jogo

Sete capturas da 0.17.0 no ETS2, aprovadas pelo usuario. O que a medicao diz:

| | p1 | mediana | topo% |
|---|---|---|---|
| Referencia (ATS, 5 imagens) | 8–12 | 11–40 | 0,00 |
| Plugin 0.17.0 (ETS2, 7 capturas) | **7–8** | 33–57 | **0,00** |

- **p1 no alvo.** O piso de preto da 0.14.0 aguenta o bloom somando luz por
  cima, que era o risco real de compor antes da curva;
- **`topo%` = 0,00 nas sete.** Somar luz nao estourou nada. E a confirmacao de
  que compor ANTES do `apply_highlight_rolloff` funciona: o ombro comprime a
  soma em vez de deixar bater no teto.

Duas divergencias em relacao a referencia que **nao sao defeito do plugin**, e
sim conteudo diferente: as capturas sao ETS2 com cabine Scania, e as
referencias eram ATS. Mediana mais alta acompanha um interior mais claro; e o
canal B dominante em quatro delas acompanha ceu encoberto de fim de tarde,
enquanto o alvo "G mais alto" saiu de cenas de luz quente. Julgar as duas
coisas juntas seria comparar iluminacoes diferentes.

O que **nao** da para afirmar por estas capturas: quanto do resultado e o bloom.
Nao ha par A/B no mesmo enquadramento com `enabled=false`, entao a aprovacao e
da imagem inteira, e nao do modulo isolado. `Insert` na posicao 6 resolveria
isso quando interessar.

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

## A ferramenta, e o limite que ela revelou

`tools/bloom_report.py` mede o perfil radial em volta da fonte mais brilhante.
Contra alvos sinteticos de raio conhecido ele acerta: recupera 0,035 como 0,038
e 0,094 como 0,100, com vies de ~7% para cima porque o primeiro anel media um
disco e nao um ponto.

**Sobre as referencias ele nao funcionou, e o aviso dele foi quem disse.** A
coluna `fontes` veio entre 4 e 9 nas cinco, e os raios entre 0,06 e 0,19 da
altura -- valores sem sentido para um kernel. O motivo: naquelas imagens o que
e claro nao e uma fonte, e o **ceu visto pela janela**, uma area grande.
Perfilar radialmente em volta dela mede o gradiente da cena, nao o
espalhamento da luz.

O que funcionou foi o perfil atravessando uma borda de alto contraste em
resolucao cheia. O limite esta escrito no cabecalho da ferramenta, para a
proxima pessoa nao gastar a mesma hora.

`validate.sh` guarda a ressalva do cfg e o limiar medido. Sem a ressalva,
alguem sobe `intensity` achando que se aproxima do alvo, quando se afasta.
