# Avaliacao do `roimehrez/photorealism` (SPE) - 0.18.2

O usuario trouxe `https://github.com/roimehrez/photorealism` perguntando o que
dava para aproveitar. Este documento registra o que foi medido, **incluindo uma
recomendacao minha que a medicao derrubou**, porque o erro e mais util que a
conclusao.

## O que o repositorio e

MATLAB + um MEX Windows. Implementa "Photorealistic Style Transfer with
Screened Poisson Equation" (Mechrez, Shechtman, Zelnik-Manor, BMVC 2017).
Recebe a saida de um style transfer e a corrige contra a foto original:

```
min  ‖w·(∇u − ∇original)‖²  +  ‖u − estilizado‖²
```

Em Lab, com `grad_weight = [5,1,1]`, resolvido como sistema esparso global
`(AᵀA)\(Aᵀb)`. Em 2560x1440 seriam 3,7 milhoes de incognitas, tres vezes.
Offline. Nada disso entra num `Present`.

## A identidade que vale a pena saber

A solucao tem forma fechada em Fourier e se reduz a:

```
u = passa-baixa(graduado) + passa-alta(original)
```

com o passa-baixa de transferencia `1/(1 + λ|ω|²)`, `λ = w²`.

Implementei o solve esparso do repositorio e a forma fechada lado a lado
(`scratchpad/spe_check.py`): **batem com erro de 1e-13**. O solve global caro e,
literalmente, um frequency split. Isso caberia num passe de shader.

## Por que o metodo, como publicado, e o oposto do que se quer aqui

Rodei a cadeia completa do plugin (perfil do log da 0.18.1) sobre uma captura
F12 real do ETS2 -- que, como o F12 nao captura o plugin, e o frame pre-grade.
Ganho de gradiente no canal L contra o original:

| faixa L | so a curva tonal | curva + realce |
| ------- | ---------------- | -------------- |
| 0-10    | 0,919            | 1,036          |
| 20-50   | 1,012-1,020      | 1,21-1,23      |
| 70-100  | 0,924            | 1,088          |
| todo    | **0,977**        | **1,153**      |

**A curva tonal quase nao distorce estrutura**: fica em ±8%, comprimindo sombra
e alta luz. Os 15,3% de ganho sao `sharpness=0.200` + `local_contrast=0.240`,
que sao deliberados e fazem parte do visual aprovado.

Aplicar o SPE derruba 99% do desvio de gradiente -- ou seja, **apaga o realce
inteiro**. E a correcao certa para style transfer, que gera estrutura falsa. A
estrutura extra daqui nao e falsa, e escolhida.

Testei tambem o meio-termo (SPE so sobre a curva, realce somado depois):
recupera micro-detalhe de sombra, 21 -> 24 niveis distintos de L, ao custo de
0,49 codigos de media e de uma cadeia de blur nova em resolucao cheia -- a
piramide do bloom nao serve, faz bright-pass com `threshold=0.850` antes de
reduzir. Refino, nao conserto: a 0.17.1 levou a sombra de 10 para 30 niveis.
Fica fora.

## A recomendacao que eu dei errada

Do peso `[5,1,1]` -- estrutura vale 5x mais em luminancia que em croma -- eu
concluí que `apply_temperature`, sendo um multiplicador RGB, distorcia a
estrutura de L. Medi contra o original e achei 1,0123 de ganho de gradiente,
erro RMS 0,094, subindo com o tint. Recomendei corrigir.

**O controle derruba isso.** Um ganho ACROMATICO de luminancia identica:

| operacao                             | ganho grad L | erro RMS |
| ------------------------------------ | ------------ | -------- |
| balanco de branco de hoje            | 1,0123       | 0,0940   |
| ganho acromatico x1,028920           | 1,0118       | 0,0921   |
| balanco com luminancia preservada    | 1,0000       | 0,0010   |

A parte atribuivel a MATIZ e `1,00046` -- **0,046%**. Praticamente todo o desvio
que eu medi era a exposicao que o vetor de balanco carrega, expressa em Lab
(L e proporcional a Y^(1/3), entao um ganho uniforme de Y muda o gradiente de L
de forma dependente do nivel). Nao havia distorcao de estrutura para corrigir.

O erro foi medir uma operacao contra o original e atribuir o desvio inteiro a
propriedade que eu estava investigando, sem rodar o controle que separa as duas
causas. O controle custou um comando.

## O defeito que a investigacao encontrou de verdade

O vetor de balanco carrega exposicao, e ninguem sabia.

```
6400K, tint 0,50  ->  balanco 0,9773 / 1,0500 / 0,9721
luminancia Rec.709 do vetor = 1,028920  =  +0,0411 EV

exposure no cfg    = -0,0300 EV
EXPOSICAO EFETIVA  = +0,0111 EV        <- sinal trocado
```

Enquanto `tint` era constante isso era um erro fixo, absorvido na calibracao.
A partir da 0.19.0 `tint` se move com o clima, e o erro se move junto:

```
varrer tint de 0,0 a 1,0 a 6400K:  +0,0004 -> +0,0807 EV  =  5,7% de brilho
varrer temperature 3000-10000K:                            0,0275 EV
```

O eixo verde-magenta e o pior dos dois porque G pesa 0,7152 dos tres. A imagem
clarearia ao ficar esverdeada e escureceria ao esfriar, sozinha. **Cor que muda
brilho e exatamente o que se le como irreal** -- e nisso o paper tem razao,
so nao pelo mecanismo que eu tinha proposto.

Correcao na 0.18.2: `apply_temperature` divide o balanco pela propria
luminancia, e os +0,0411 EV foram para `exposure` no cfg (-0,09 -> -0,0488697).
Os dois sao multiplicacao em linear e comutam, entao a saida nao muda:
**9 pixels de 11.059.200 diferem em 1 codigo (0,0001%)**, que e arredondamento
de quantizacao. A deriva ao varrer tint cai de 5,85% para 0,82% -- o que sobra
e croma de verdade passando pela curva de potencia e pelo ombro, nao o vetor.

## Saldo

- **Aproveitado**: nada de codigo. Do paper ficou a pergunta certa -- "esta
  operacao mexe em brilho quando deveria mexer so em cor?" -- que encontrou um
  defeito real em outro lugar.
- **Rejeitado**: o SPE em si, que apagaria o realce aprovado; e o solve
  esparso, que nao cabe em tempo real de nenhum jeito.
- **Adiado**: o SPE dirigido so a curva tonal, se algum dia sombra e alta luz
  virarem o problema principal e houver orcamento para um blur em resolucao
  cheia.
