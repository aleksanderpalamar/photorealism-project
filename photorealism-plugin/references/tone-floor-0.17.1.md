# Piso do preto medido, 0.17.1

Cinco referencias do ATS (3840x2160, interior de cabine) contra quatro capturas
da 0.17.0 (1920x1080). A capture `12-45-09` e externa, e as referencias sao
todas de interior: **mediana e media nao comparam entre os dois conjuntos**. O
que sobrevive a diferenca de conteudo, e o que esta medido aqui, sao as
ancoras, as razoes por decil, o perfil de borda e a estrutura da sombra.

## O que ja batia na 0.17.0

|                          | referencias   | 0.17.0        |
| ------------------------ | ------------- | ------------- |
| % >= 250 nos tres canais  | 0,0000        | 0,0000        |
| Y linear p100            | 0,80 a 0,95   | 0,90 a 0,95   |
| cor do branco R/G        | 0,955 a 1,002 | 0,945 a 0,958 |
| cor do branco B/G        | 0,976 a 1,009 | 1,000 a 1,018 |
| canal mais alto          | G             | G             |
| borda claro para escuro  | cai em 2 px   | cai em 2 px   |

O ombro da 0.14.0 e o balanco do branco estao certos. O bloom da 0.17.0 em
`intensity=0.02` esta de fato invisivel, como pretendido.

## O piso

| arquivo         | piso (1% mais escuro) | R/G   | B/G   | niveis <12 | desvio <12 | R=G=B exato |
| --------------- | --------------------- | ----- | ----- | ---------- | ---------- | ----------- |
| REF 23-33-14    | 2,1 / 5,8 / 5,2       | 0,363 | 0,900 | 28         | 4,01       | 0,00%       |
| REF 23-47-51    | 1,6 / 5,7 / 5,1       | 0,287 | 0,898 | 31         | 3,96       | 0,00%       |
| REF 11-12-15    | 5,8 / 9,1 / 10,6      | 0,638 | 1,161 | 24         | 3,10       | 0,53%       |
| REF 11-12-25    | 5,7 / 9,0 / 10,5      | 0,630 | 1,164 | 26         | 3,37       | 0,24%       |
| REF 15-56-22    | 3,8 / 7,2 / 7,6       | 0,524 | 1,056 | 26         | 3,11       | 0,77%       |
| 0.17 12-45-09   | **8,0 / 8,0 / 8,0**   | 1,000 | 1,000 | 13         | 1,19       | **71,8%**   |
| 0.17 12-45-37   | **9,0 / 9,0 / 9,0**   | 1,000 | 1,000 | 12         | 0,70       | **89,9%**   |
| 0.17 12-45-47   | **9,0 / 9,0 / 9,0**   | 1,000 | 1,000 | 13         | 0,83       | **87,3%**   |
| 0.17 12-45-55   | **9,0 / 9,0 / 9,0**   | 1,000 | 1,000 | 12         | 0,85       | **88,8%**   |

Exatos, nao aproximados: o 1% mais escuro inteiro no mesmo codigo.

## A causa

`max((color - pivot) * Contrast + pivot, 0.0)` com `Contrast` acima de 1 manda
todo valor abaixo de `pivot*(Contrast-1)/Contrast` para negativo, e o clamp
junta o conjunto no mesmo zero. Com o perfil aprovado (pivo 0,18, Contrast
1,07) esse limiar e **0,01178** na entrada do passo. Simulada a cadeia inteira,
a entrada da cadeia que ainda escapa e **0,0147 linear, 32 em 255**:

```
entrada linear 0.002 (sRGB  6.6) -> saida 8.90/255
entrada linear 0.010 (sRGB 25.5) -> saida 8.90/255
entrada linear 0.012 (sRGB 28.6) -> saida 8.90/255
entrada linear 0.015 (sRGB 32.7) -> saida 10.49/255
```

Previsto 8,90 -- medido 8 e 9, chapados. O `black_lift` vinha depois do clamp:
so escolhia o valor do plato, nunca poderia recuperar a estrutura.

## A correcao, simulada antes de escrever o shader

`pivot * pow(max(color, 1e-6) / pivot, Contrast)` mais `black_lift` de tres
componentes:

| entrada sRGB | 0.17.0            | R/G   | 0.17.1              | R/G   | B/G   |
| ------------ | ----------------- | ----- | ------------------- | ----- | ----- |
| 0            | 8,90/8,90/8,90    | 1,000 | **2,11/5,83/5,24**  | 0,362 | 0,898 |
| 12           | 8,90/8,90/8,90    | 1,000 | 2,53/6,69/5,61      | 0,377 | 0,838 |
| 18           | 8,90/8,90/8,90    | 1,000 | 8,64/12,96/11,60    | 0,667 | 0,896 |
| 25           | 8,90/8,90/8,90    | 1,000 | 17,38/20,69/19,34   | 0,840 | 0,935 |
| 45           | 33,37/35,23/33,17 | 0,947 | 40,04/42,52/40,87   | 0,942 | 0,961 |
| 128          | 128,57/131,75/... | 0,976 | 128,19/131,66/...   | 0,974 | 0,973 |
| 250          | 238,73/242,57/... | 0,984 | 242,67/245,84/...   | 0,987 | 0,986 |

Piso alvo medido: 2,11/5,82/5,24, R/G 0,363, B/G 0,900. Niveis distintos na
entrada 0-40: **10 na 0.17.0, 30 na 0.17.1**, contra 24 a 31 medidos nas
referencias.

A tabela acima e a base sozinha. As tres camadas estao **sempre somadas** --
nao ha deteccao de clima no plugin -- entao o piso que a tela mostra e a soma,
e ela mira a mediana por canal das cinco referencias: **4/7/8 em 255**, R/G
0,525 e B/G 1,056, dentro da faixa medida (R/G 0,287 a 0,638, B/G 0,898 a
1,164). A base leva o piso de tempo claro e `rain_overcast` completa a
diferenca; mirar a soma nas duas de neblina poria o piso permanente no extremo
da faixa em vez do centro dela.

O topo sobe 2 a 4 codigos (em 220, de 220,8 para 225,0). Cabe no ombro --
`%>=250` continua 0,0000 -- mas pede uma rodada A/B de `exposure` em jogo.

**Nao verificado por medicao:** a potencia preserva RAZAO entre canais onde a
reta preservava DIFERENCA, entao em conteudo saturado ela abre um pouco a
saturacao nos meios-tons. Nos degrades neutros acima isso nao aparece por
construcao (R=G=B), e nao da para prever a partir das capturas porque a
informacao abaixo de 32/255 foi destruida nelas. Confirmar em jogo; `saturation`
esta em 0,97 e e o ajuste se precisar.

## O que continua faltando para o mesmo visual

- **grao.** Referencias 0,95 a 1,14 niveis de ruido por canal em regiao clara e
  lisa, capturas 0,16 a 0,32. Quatro a seis vezes menos. Passe de dither na
  saida, isolado desta versao;
- **raios de sol.** Confirmados na `11-12-15`, saindo do disco solar e
  projetados no quebra-sol escuro. E a 0.18.0 do ROADMAP.

## Correcao de processo

Os tres criterios do `grade_report.py` -- p1 na faixa 6-12, G mais alto,
`topo%` baixo -- **passaram nas quatro capturas** com os dois defeitos
presentes. `p1` nao distingue um piso com estrutura de um plato no mesmo valor.
Faltam ali: niveis distintos abaixo de 12/255, fracao de pixels escuros com
R=G=B exato, e a razao R/G do piso.
