# O vies do estimador de piso, 0.17.2

A 0.17.1 acertou a implementacao e errou o alvo. Este arquivo mede o erro.

## O estimador

O piso do preto foi medido nas cinco referencias e nas capturas pelo mesmo
caminho: converte para linear, ordena por luminancia, pega a media do 1% mais
escuro. Chamado aqui de estimador de cauda.

Ele e valido quando a imagem nao tem grao. Teste direto: o piso do plugin e uma
constante conhecida, o proprio `black_lift` efetivo, que em 0.17.1 vale
0,001150/0,002192/0,002313 linear ou 3,79/7,22/7,62 em 255. Nas doze capturas
limpas o estimador devolve 3,96/6,89/7,97 -- erro de 0,17 / -0,33 / 0,35.

## O vies

Com grao ele deixa de ser valido, e de um jeito assimetrico.

A amostra e escolhida ordenando por LUMINANCIA, que pesa 0,2126 R, 0,7152 G e
0,0722 B. Num pixel com grao, o que decide se ele entra no 1% mais escuro e
quase inteiramente o ruido no canal G. A amostra fica enviesada para excursoes
negativas de G, e so de G.

Medido por injecao de ruido gaussiano nas mesmas doze capturas:

| desvio do grao | cauda medida        | deslocamento          |
| -------------- | ------------------- | --------------------- |
| 0,0 (limpo)    | 3,96 / 6,89 / 7,97  | -                     |
| 1,0            | 3,95 / 6,36 / 8,10  | -0,02 / -0,53 / +0,13 |
| 1,7            | 3,84 / 5,59 / 8,29  | -0,12 / -1,30 / +0,32 |
| 2,1            | 3,78 / 5,06 / 8,40  | -0,18 / -1,83 / +0,43 |
| 2,5            | 3,74 / 4,49 / 8,50  | -0,22 / -2,41 / +0,53 |

R e B quase nao se movem, como o mecanismo preve. O G desce quase um codigo por
unidade de desvio.

As cinco referencias tem grao de desvio ~2,1 e as capturas do plugin ~0,18. A
0.17.1 comparou os dois conjuntos com o mesmo estimador sem corrigir nada, e
por isso leu as referencias como tendo o G quase dois codigos mais baixo do que
tem.

## O alvo corrigido

Aplicando o deslocamento de -0,18 / -1,83 / +0,43 as cinco:

| referencia   | cauda crua         | corrigida          | R/G   | B/G   |
| ------------ | ------------------ | ------------------ | ----- | ----- |
| 23-33-14     | 3,17 / 4,70 / 6,46 | 3,35 / 6,53 / 6,03 | 0,513 | 0,924 |
| 23-47-51     | 2,68 / 4,32 / 6,65 | 2,86 / 6,15 / 6,22 | 0,465 | 1,011 |
| 11-12-15     | 7,46 / 7,84 / 11,2 | 7,64 / 9,67 / 10,8 | 0,790 | 1,115 |
| 11-12-25     | 6,81 / 7,62 / 11,2 | 6,99 / 9,45 / 10,7 | 0,740 | 1,137 |
| 15-56-22     | 4,60 / 6,01 / 8,25 | 4,78 / 7,84 / 7,82 | 0,610 | 0,997 |
| MEDIANA      | 4,60 / 6,01 / 8,25 | 4,78 / 7,84 / 7,82 |       |       |

Faixa corrigida: R/G de 0,465 a 0,790, B/G de 0,924 a 1,137.

## A troca de sinal

Resolvendo o lift contra os dois alvos:

|                | R        | G         | B       |
| -------------- | -------- | --------- | ------- |
| alvo cru       | +2,3%    | **-24,4%**| -1,9%   |
| alvo corrigido | +21,6%   | **+13,1%**| -2,0%   |

O G trocava de sinal. Aplicar a primeira resposta teria escurecido o canal que
precisava subir, e a diferenca entre as duas e 37 pontos percentuais.

## Como os tres numeros saem do alvo

Nao por conversao direta. O piso medido numa captura e o lift mais o que a cena
poe por cima, cerca de 0,6 codigo. A etapa do shader e afim e exatamente
inversivel:

    out = lift + (1 - lift) * c        ->        c = (out - lift) / (1 - lift)

Recuperado `c` pixel a pixel no 1% mais escuro das doze capturas, o lift novo
sai por bisseccao por canal ate a cauda prevista bater no alvo:

    efetivo   0.001398 / 0.002480 / 0.002268   ->  piso 4,78 / 7,84 / 7,82
    base      0.001017 / 0.001982 / 0.001888   ->  piso 3,35 / 6,53 / 6,22
    rain      0.000381 / 0.000498 / 0.000380

A base sai da mediana das tres referencias de tempo claro corrigidas, pelo mesmo
caminho da 0.14.0: codigo/255/12,92. Os deltas de `rain_overcast` completam a
soma, porque a camada esta sempre ligada.

## Dois estimadores que discordam, e por que a cauda ganha

Um segundo estimador -- media dos pixels crus em blocos escuros e planos, sem
selecao de cauda e sem mistura espacial -- da outra leitura:

|          | referencias        | 0.17.1             |
| -------- | ------------------ | ------------------ |
| plano    | 7,24 / 13,9 / 12,8 | 6,54 / 11,0 / 11,9 |
| R/G, B/G | 0,520 / 0,918      | 0,593 / 1,083      |

Os dois concordam que os niveis tem que subir e que B/G tem que cair. Discordam
no sinal de R/G.

A cauda decide porque `black_lift` e um parametro de PISO, e a cauda e o unico
dos dois que mede piso: o estimador plano mede o nivel tipico de area escura,
que e piso mais conteudo. E a cauda tem validacao independente -- recupera o
piso conhecido do plugin dentro de 0,35 codigo.

Fica registrado que o estimador plano, com a correcao aplicada, ainda poe R/G
andando para o lado errado (0,593 para 0,614, contra 0,520 da referencia). Se a
proxima captura mostrar a sombra vermelha demais, e por aqui que se comeca.

## O que este arquivo nao mede

O deslocamento foi medido injetando ruido gaussiano independente por canal. Se
o grao das referencias for correlacionado entre canais, a magnitude muda. O
mecanismo -- selecao por luminancia enviesar so o G -- nao muda.
