# Observador de cena 0.18.0

Este modulo nao altera um pixel. Ele mede o frame **antes** do grade e escreve
quatro numeros no log. Existe para produzir a evidencia que falta para a
adaptacao por condicao -- e a adaptacao so entra depois que essa evidencia
existir.

## Por que ele precisa existir

A calibracao de cor de hoje e uma media de condicoes que nao se parecem.

As cinco imagens em `~/Documentos/amtrucks *.png`, que varios documentos deste
repositorio chamam de "as cinco referencias do ATS", foram abertas e medidas
aqui pela primeira vez. Elas nao sao cinco fotos da mesma coisa:

| ref      | condicao                       |
| -------- | ------------------------------ |
| 23-33-14 | encoberto, luz chapada         |
| 23-47-51 | anoitecer, farois ligados      |
| 11-12-15 | sol baixo com neblina          |
| 11-12-25 | sol baixo com neblina, 10s depois |
| 15-56-22 | dia claro, ceu azul            |

Com um `tint` unico nao ha como acertar as cinco. O valor efetivo de hoje,
0,50, cai entre o alvo de dia claro e o de encoberto e erra os dois. Nao e
descuido: e o unico numero possivel quando se tenta cobrir cinco condicoes com
um so.

**Estas imagens sao do ATS e sao SAIDA.** Servem para descrever as condicoes e
para dizer que aparencia o usuario quer em cada uma. Nao servem como limiar: o
detector le o frame pre-grade do **ETS2**, que e o jogo alvo -- outro jogo,
outra cena, outra iluminacao. Os limiares saem das linhas `Cena 0.18.0:` do log
do proprio ETS2.

## As quatro features

Medidas pelo mesmo caminho que o observador usa em jogo: media de area ate a
largura cair para ~80 pixels, luma Rec.709 sobre o **codigo** sRGB, ceu = os
10% mais claros da metade superior do quadro.

| ref      | condicao   | ceu R/B | mediana | p90-p10 | saturacao |
| -------- | ---------- | ------- | ------- | ------- | --------- |
| 23-47-51 | anoitecer  | 0,915   | 13,1    | 55,0    | 0,421     |
| 23-33-14 | encoberto  | 0,967   | 21,2    | 131,2   | 0,338     |
| 11-12-25 | sol/neblina| 1,005   | 21,0    | 102,4   | 0,260     |
| 11-12-15 | sol/neblina| 1,000   | 30,3    | 95,2    | 0,212     |
| 15-56-22 | dia claro  | 0,938   | 43,9    | 157,7   | 0,183     |

Nenhuma das quatro separa sozinha:

- **ceu R/B** confunde neblina com sol baixo (1,005 contra 1,000);
- **mediana** confunde encoberto com neblina (21,2 contra 21,0);
- **p90-p10** confunde sol baixo com neblina (95,2 contra 102,4);
- **saturacao** confunde sol baixo com dia claro (0,212 contra 0,183).

Juntas, normalizadas pela dispersao da populacao, o par de condicoes
**diferentes** mais proximo fica a **1,65**. O par mais proximo de todos e
`11-12-15 x 11-12-25`, a **1,07** -- e esse par e a mesma condicao com dez
segundos de diferenca e a camera apontada para lados diferentes. Ou seja: a
dispersao dentro de uma condicao e menor que a distancia entre condicoes, que
e exatamente a propriedade necessaria.

`tests/scene_features_test.cpp` guarda os dois numeros. Se alguem trocar uma
feature por outra que pareca mais esperta, o teste cai.

## A margem e apertada, e isso decide o desenho

1,65 contra 1,07 e uma margem de 1,5x. Nao e folgada. A consequencia direta:
**nenhum detector montado em cima disto pode usar classe dura.** Uma amostra
que caia no meio entre duas condicoes trocaria de classe de um frame para o
outro e a cor saltaria. A adaptacao tem que interpolar continuamente, e ainda
assim suavizar com constante de tempo de segundos e histerese.

Virar a cabine nao pode trocar o grade. Foi por isso que a regiao de ceu ficou
sendo "os mais claros da metade superior" e nao "a linha de cima": numa vista
interna os mais claros de cima sao o para-brisa, numa externa sao o ceu, e nos
dois casos e a mesma superficie iluminante. E a feature que corria mais risco
na troca de camera, e e por isso que ela foi definida assim.

## Onde ele mede, e por que exatamente ali

`postprocess.cpp`, colado no `CopyResource(scene_texture_, back_buffer)`.
Naquele ponto `scene_texture_` acabou de receber o frame do jogo e nenhum passe
nosso escreveu nele.

Medir a saida fecharia uma realimentacao: a cor seria funcao das features e as
features funcao da cor, e a imagem caminharia sozinha sem que nada no cfg
tivesse mudado. `tools/validate.sh` verifica a linha da chamada por isso.

## Custo

`GenerateMips` do hardware sobre uma copia da cena, uma vez a cada
`interval_frames` (30 por padrao, ~0,5s a 60fps), e leitura de ~80x45 = 3600
pixels por amostra. A volta e assincrona: `D3D11_QUERY_EVENT` mais
`GetData` com `DONOTFLUSH`, e o `Map` so acontece quando a copia ja chegou. Um
`Map` bloqueante ali custaria a latencia inteira do pipeline.

Sao dois slots de staging. Se o anterior ainda nao voltou, a amostra e
descartada em vez de esperar -- condicao de tempo muda em minutos e a proxima
vem em menos de um segundo.

## Como isto vira calibracao

Jogar ETS2 normalmente, passando por horarios e climas diferentes. Cada 30
segundos o log ganha uma linha:

```
Cena 0.18.0: ceu_R/B=... mediana=... faixa_p90-p10=... saturacao=... media=... amostra=...
```

Com essas linhas rotuladas pela condicao que estava na tela, os limiares do
detector passam a ser medidos no jogo certo, em vez de transportados de uma
saida do ATS.

## O que este documento NAO afirma

- Nao afirma que as quatro features separam condicoes no **ETS2**. A tabela
  acima e do ATS. A separacao no ETS2 e a proxima medicao, e e para isso que o
  log existe.
- Nao afirma nada sobre chuva. Nenhuma das cinco referencias tem chuva, e a
  camada que leva o nome `rain_overcast` nunca teve alvo medido.
- Nao ha detector ainda. Este modulo so mede.

## Adendo 0.18.1: o modulo saiu desligado

O primeiro log de jogo da 0.18.0 tem **663.486 linhas** de
`Observador de cena 0.18.0 inativo: formato 90 ... nao suportados` e **zero**
linhas `Cena 0.18.0:`. Seis sessoes, 67 MB, nenhuma medida.

Formato 90 e `B8G8R8A8_TYPELESS`. Nao e exotico: e o que
`ensure_frame_resources` cria de proposito para a copia da cena, para poder
pendurar nela uma SRV sRGB. O observador recusava o formato do caminho
principal.

Vale registrar por que nada acusou. Existiam:

- `build.sh` compilando os dois DLLs -- e compilava, o codigo estava correto;
- `validate.sh` com uma guarda escrita para este modulo -- e ela fixava a
  **linha da chamada**, `scene_observer_.observe(device_, context_, scene_texture_)`,
  que estava certa;
- `scene_features_test.cpp` com cinco grupos de assert -- e todos passavam,
  porque testavam a matematica, que estava certa.

O que ninguem testava era a **tabela de formatos entre a chamada e a
matematica**, porque ela morava dentro do `.cpp` junto com os tipos do D3D11,
fora do alcance de um teste que compila em Linux. A licao que ficou no
repositorio: a tabela virou `src/scene_formats.hpp` em `unsigned`, e
`tests/scene_formats_test.cpp` fixa `is_readable(kB8G8R8A8Typeless)`.

**O mesmo defeito estava numa segunda tabela.** `depth_copy_formats` listava
so os formatos `D*`, e o ETS2 declara o depth ora como 20
(`D32_FLOAT_S8X24_UINT`), ora como 19 (`R32G8X24_TYPELESS`). Nas tres sessoes
mais recentes veio 19, e o SSAO e o resolve temporal ficaram desligados a
sessao inteira -- o candidato recusado sendo o melhor dos dois, ja que 19 vem
com `bind_flags=0x48` e o 20 com `0x40`.

Duas tabelas, o mesmo erro: enumerar as variantes tipadas e esquecer o pai
TYPELESS. Vale procurar a terceira antes que um log a encontre.

## Adendo 0.18.1: o que o log realmente mediu

Nenhuma feature de cena, porque o modulo estava desligado. Mas as seis
sessoes valem como medida do resto:

- **Custo**: media de 1,414 ms em 1.103 amostras, pior media de janela
  3,326 ms, zero amostras descartadas.
- **Hooks**: `state=nosso-hook-externo` em todas as fases de auditoria, nas
  seis sessoes, com o overlay da Steam estabilizado antes da instalacao.
- **F12**: so a linha de registro do `ISteamScreenshots`, nenhuma captura, nas
  seis. O diagnostico continua o da 0.18.0 e continua sem correcao.

E vale registrar o que o usuario viu. Ele aprovou o resultado em jogo -- e nas
tres sessoes mais recentes o que estava na tela era **so o passe de cor mais o
bloom**, sem SSAO e sem resolve temporal. Isso e informacao sobre a
calibracao, nao so sobre o bug: o que agradou nao dependia da oclusao.
