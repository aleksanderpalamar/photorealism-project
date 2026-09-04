# Primeira medicao de cena no ETS2 - 0.18.1

Uma sessao de 20 minutos, 40 amostras `Cena 0.18.0:`, log de 223 linhas.
E a primeira evidencia de cena vinda do **jogo alvo**. Tudo que a 0.18.0
afirmava sobre separacao de condicoes vinha do ATS e era, por construcao,
transportado.

## O numero que decide tudo

|            | ETS2 (36 amostras de jogo) | ATS (5 referencias) | razao |
| ---------- | -------------------------- | ------------------- | ----- |
| ceu R/B    | 0,695 a 1,075 (**0,380**)  | 0,915 a 1,005 (0,090) | 4,2x |
| mediana    | 28,3 a 72,8 (44,5)         | 13,1 a 43,9 (30,8)  | 1,4x  |
| p90-p10    | 68,0 a 200,6 (132,6)       | 55,0 a 157,7 (102,7)| 1,3x  |
| saturacao  | 0,110 a 0,338 (0,228)      | 0,183 a 0,421 (0,238)| 1,0x |

**O eixo quente-frio no ETS2 e 4,2 vezes mais largo que no ATS.** Um limiar
tirado do ATS -- por exemplo o 0,938 do "dia claro" -- cairia no meio da faixa
de ceu azul do ETS2 e classificaria quase todo quadro de dia limpo como outra
coisa. A correcao do usuario ("o foco e o ETS2, so usei o ATS pela coloracao
que eu tinha gostado") nao era so de escopo: era numericamente necessaria.

As outras tres features tem faixas parecidas nos dois jogos. So o eixo de cor
diverge, e e justamente o que a 0.19.0 vai controlar.

## A hora do dia explica 58% da variacao, e o jogo ja a renderiza

Regressao do ceu R/B contra o tempo real de sessao:

```
inclinacao = +0,0152 por minuto     r = +0,762     r2 = 0,581
em 19 minutos: +0,284, que e 75% da amplitude observada
```

Isto e medido **antes** do grade. Ou seja o proprio ETS2 esquenta o ceu ao
longo do dia, e sozinho ja produz tres quartos de toda a variacao de cor da
sessao. Confirma no jogo certo o cuidado que o ROADMAP registrava a partir do
ATS: **somar uma rampa de relogio por cima conta duas vezes.** O alvo da
0.19.0 e o ajuste que falta em cada condicao, nao uma rampa artistica.

## O que sobra depois da hora do dia e ruido de quadro, nao condicao

Residuo da regressao: desvio de **0,069**, contra amplitude util de 0,380.

Autocorrelacao do residuo:

```
30 s: +0,26     60 s: +0,09     90 s: +0,14     120 s: -0,06     180 s: -0,25
```

Cai para perto de zero em menos de um minuto. Um residuo que decorrelaciona
tao rapido nao e mudanca de tempo -- e a camera e a curva da estrada mudando
o que entra na regiao de ceu. E exatamente o modo de falha que a 0.18.0 previu
sem poder medir: *virar a cabine nao pode trocar o grade*. Agora tem numero:
oscilacoes de +-0,10 entre amostras vizinhas, 1,4 desvios, dentro de um minuto.

Como decorrelaciona rapido, mediar funciona. O compromisso, medido:

| janela | ruido | atraso | erro da tendencia | erro total |
| ------ | ----- | ------ | ----------------- | ---------- |
| 1 min  | 0,049 | 0,5 min| 0,008             | 0,049      |
| **2 min** | 0,034 | 1,0 min| 0,015          | **0,038**  |
| **3 min** | 0,028 | 1,5 min| 0,023          | **0,036**  |
| 4 min  | 0,024 | 2,0 min| 0,030             | 0,039      |
| 8 min  | 0,017 | 4,0 min| 0,061             | 0,063      |

**A constante de tempo da 0.19.0 e de 2 a 3 minutos**, com erro total de ~0,036
= 9,5% da amplitude util. Nao "segundos", como o ROADMAP dizia por estimativa:
abaixo de um minuto o ruido de camera domina, acima de quatro a hora do dia
foge. A curva e rasa entre 2 e 4 minutos, entao a escolha nao e critica --
mas 30 segundos e errado por um fator de dois.

## As quatro features nao sao redundantes

Correlacao entre elas nas 36 amostras de jogo:

|           | ceu R/B | mediana | p90-p10 | saturacao |
| --------- | ------- | ------- | ------- | --------- |
| ceu R/B   |  1,00   | -0,29   | -0,31   |  +0,34    |
| mediana   | -0,29   |  1,00   | +0,46   | -0,50     |
| p90-p10   | -0,31   | +0,46   |  1,00   | -0,35     |
| saturacao | +0,34   | -0,50   | -0,35   |  1,00     |

Maior magnitude: 0,50. O eixo de cor e quase independente dos tres de
estrutura (|r| <= 0,34), que e a propriedade que se queria. A `media`, que o
log traz como diagnostico e nao como feature, e redundante: 0,80 com a mediana
e 0,78 com a faixa. Nao vale promover a quinta feature.

## 4 das 40 amostras nao sao jogo, e um detector tem que descarta-las

```
10:43:54  R/B=1,471  mediana=  0,0  faixa=  0,0  sat=0,019  media=  4,7
10:44:54  R/B=1,078  mediana=115,2  faixa=117,8  sat=0,076  media=108,5
10:46:57  R/B=1,000  mediana=  0,0  faixa=  0,0  sat=0,000  media=  0,0
11:03:33  R/B=1,019  mediana= 66,9  faixa=103,5  sat=0,078  media= 75,6
```

Carregamento, fade e tela de mapa. **10% das amostras**, e a primeira e a mais
perigosa: quadro quase preto devolvendo R/B = 1,471, o valor mais quente da
sessao inteira, puro ruido de divisao com denominador minusculo. Alimentar
isso num detector jogaria o grade para o extremo quente no meio de um
carregamento.

O caso 10:46:57 saiu certo por desenho: quadro totalmente preto, sem regiao de
ceu utilizavel, devolveu o **neutro 1,000** em vez de zero. Foi previsto no
teste 3 de `scene_features_test.cpp`. Falta a guarda para o quase-preto.

Porta que separou os dois grupos nestes dados: `p90-p10 > 20` **e**
`saturacao > 0,09`. Ela e o ponto de partida da 0.19.0, nao a resposta final --
36 amostras de uma sessao nao cobrem tunel, chuva forte nem noite fechada, que
sao justamente os casos em que uma porta assim erra.

## O resto da sessao

- **Custo**: 2,082 ms de media em 118 janelas, pior janela 3,696 ms, pico
  6,025 ms, zero amostras descartadas. Contra 1,414 ms da 0.18.0 **sem SSAO nem
  resolve temporal** -- entao os dois passes que voltaram custam ~0,67 ms. O
  pico caiu de 23,31 ms para 6,03 ms.
- **Amostra**: 3600 pixels nas 40 medidas, sem uma unica variacao. Os dois
  slots de staging deram conta e nada foi descartado por leitura em voo.
- **Hooks**: `state=nosso-hook-externo` nas 9 fases de auditoria.
- **Erros**: nenhum. As unicas linhas que casam com /erro/ sao o campo
  `aspect_error=` do diagnostico de depth.
- **F12**: so a linha de registro do `ISteamScreenshots`, nenhuma captura. O
  diagnostico segue o da 0.18.0 e continua sem correcao.

## O que esta sessao NAO mede

Uma sessao, um horario, um trecho de estrada. Nao ha aqui chuva, nevoeiro,
noite fechada, tunel nem neve. A tendencia de +0,0152/min e a desta sessao e
nao e uma constante do jogo -- depende da escala de tempo do perfil. E as
condicoes nao estao rotuladas: sabe-se que a cor esquentou, nao *o que* estava
na tela. Sem rotulo, isto calibra a suavizacao e a porta de jogo, mas nao as
ancoras de `temperature`/`tint` por condicao.

---

# Segunda sessao, 61 minutos, 0.18.2 - 2026-09-04

119 amostras de jogo contra as 36 da primeira. Ela **corrige duas conclusoes**
tiradas da sessao curta, e por isso vale mais que o triplo de dados.

Confirmacoes primeiro: `ganho_luma=1.000000 (+0.0000 EV)` no runtime, SSAO e
resolve temporal ativos, zero `incompativel com copia`, custo 1,748 ms de media
em 368 janelas (pico 6,169 ms, zero descartadas), e o "Perfil efetivo" agora
diz `exposure=0.011`, que e a exposicao de verdade.

## As faixas eram estreitas demais

| feature   | 61 min: min - p25 - mediana - p75 - max | 20 min |
| --------- | --------------------------------------- | ------ |
| ceu R/B   | 0,566 · 0,852 · 0,975 · 1,147 · **1,503** | 0,695 a 1,075 |
| mediana   | 11,8 · 23,7 · 30,8 · 39,5 · 84,2         | 28,3 a 72,8 |
| p90-p10   | 45,1 · 60,6 · 74,4 · 103,3 · 215,5       | 68,0 a 200,6 |
| saturacao | 0,135 · 0,197 · 0,240 · 0,293 · 0,361    | 0,110 a 0,338 |

A amplitude do ceu R/B passou de 0,380 para **0,937**. A primeira sessao era um
recorte estreito, e qualquer limiar tirado so dela estaria errado. Contra o ATS
(0,090) a diferenca agora e de **10x**, nao 4x.

## CORRECAO 1: a tendencia de hora do dia nao se sustenta

|                | 20 min          | 61 min          |
| -------------- | --------------- | --------------- |
| inclinacao     | +0,0152 /min    | **-0,0024 /min**|
| r2             | 0,581           | **0,040**       |

A primeira sessao dizia que a hora do dia explicava 58% da variacao de cor. Na
segunda ela explica **4%**. O documento acima ja alertava que "a tendencia e a
desta sessao e nao e uma constante do jogo" -- estava certo em alertar, e o
alerta virou fato. **Nao ha rampa de relogio a modelar.** O que a 0.19.0 tem de
seguir e a condicao, nao o horario; a cor da hora o jogo ja renderiza e nao
precisa de ajuda nossa.

## CORRECAO 2: o residuo nao e ruido de camera

Autocorrelacao do residuo:

```
20 min:  30s=+0,26  60s=+0,09  120s=-0,06  180s=-0,25     -> some em <1 min
61 min:  30s=+0,65  60s=+0,45  120s=+0,38  180s=+0,45  240s=+0,41  -> persiste
```

Na sessao curta o que sobrava era a camera virando e descorrelacionava em menos
de um minuto. Na longa ele ainda vale +0,41 aos quatro minutos: **e condicao
mudando, e e sinal, nao ruido.** Decompondo, 44% da variancia e rapida
(sd 0,138) e o resto e lenta (sd 0,147). As duas sao grandes.

## A janela, medida sem circularidade

Uma primeira tentativa deu "8 minutos", e estava errada por construcao: eu
definira o sinal lento como a propria media de 8 minutos, entao a janela de 8
minutos nao perdia nada. O criterio honesto e o **erro de previsao causal** --
estimar a condicao a partir so do passado e ser julgado pela amostra seguinte,
que e exatamente o que o detector faz.

| janela | erro, 20 min | erro, 61 min |
| ------ | ------------ | ------------ |
| 0,5 min| 0,0774       | 0,1698       |
| 1,5 min| **0,0667**   | 0,1629       |
| 3 min  | 0,0765       | 0,1647       |
| 4 min  | 0,0818       | **0,1605**   |
| 8 min  | 0,1042       | 0,1795       |

O minimo cai em 1,5 min numa e 4 min na outra, e a curva e **rasa** entre as
duas: na sessao longa a diferenca entre 0,5 e 6 minutos e de 6%. A recomendacao
de **2 a 3 minutos** sobrevive, agora por ser o meio de uma regiao chata e nao
por um minimo agudo.

O numero desconfortavel e o piso: **o erro fica em 17-18% da amplitude nas duas
sessoes**, independentemente da janela. Uma feature suavizada nao determina a
condicao melhor que isso. As quatro juntas precisam fazer melhor, e isso ainda
nao foi medido.

## A porta de jogo pegou 4 de 123, de novo

```
14:33:14  R/B=1,596  mediana= 0,0  faixa=  0,0  sat=0,021   quadro preto
14:39:17  R/B=0,890  mediana= 0,0  faixa=  2,0  sat=0,156   quase preto
15:31:04  R/B=0,981  mediana=59,1  faixa=103,7  sat=0,076   mapa/menu
15:34:35  R/B=0,981  mediana=65,0  faixa=120,1  sat=0,068   mapa/menu
```

Mesma proporcao (3,3%) e mesmo pior caso: **um quadro preto devolvendo R/B =
1,596, o valor mais quente da sessao**, agora acima do maximo de jogo (1,503).
Duas sessoes, duas vezes. A porta `p90-p10 > 20 e saturacao > 0,09` e
necessaria.

Fica uma duvida em aberto: `14:39:17` tem saturacao 0,156, que e valor de jogo.
Se aquilo era um tunel ou noite fechada e nao um fade, a porta esta descartando
uma condicao real em vez de lixo. **So o rotulo do usuario resolve.**

## O que continua faltando

O mesmo de antes, e agora e o unico bloqueio: **as linhas rotuladas pela
condicao na tela.** Ha 155 amostras de jogo somando as duas sessoes, cobrindo
ceu R/B de 0,566 a 1,503, e nao se sabe qual delas era sol, chuva, tunel ou
anoitecer. Sem isso da para calibrar a suavizacao e a porta -- e foi o que se
fez -- mas nao as ancoras de `temperature`/`tint` por condicao, que sao o
produto da 0.19.0.
