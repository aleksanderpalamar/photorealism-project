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
