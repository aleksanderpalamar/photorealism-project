# RTGI 0.13.3 - acumulacao temporal do GI

Estado: entregue. Continua `rtgi-ambient-escape-0.13.2.1.md`, que fez os raios
que escapam devolverem ambiente em vez de preto. Precede a 0.13.4, o denoiser
bilateral.

## O problema

Quatro raios por pixel e ruido, e ate aqui esse ruido ia inteiro para a tela.
A cada frame cada raio sorteia uma direcao nova no hemisferio, acerta alguma
coisa ou nao acerta, e o pixel cintila. Nao e defeito de implementacao: e o que
Monte Carlo faz com quatro amostras. A saida nao e aumentar `ray_count` -- o
custo e linear e a variancia cai com a raiz -- e sim reaproveitar os frames que
ja foram tracados.

## Duas entregas

### 1. Acumulacao

`PSRtgiTemporal`, terceiro entry point de `shaders/rtgi.hlsl`, roda na
resolucao do RTGI entre a marcha e a composicao. Le o GI atual, a historia, o
depth atual e o depth do frame anterior, e devolve
`lerp(atual, historia_clampada, aceito)`.

Os quatro parametros marcados `INERTE ate a 0.13.3` no cfg desde a 0.12.0
finalmente chegam ao cbuffer:

| chave | papel |
|---|---|
| `history_weight` | teto de quanto do frame anterior entra |
| `depth_rejection` | diferenca relativa de profundidade que ja e outra superficie |
| `normal_rejection` | angulo entre a normal de agora e a de antes |
| `color_rejection` | quanto a cor pode ter mudado antes de a historia ser suspeita |

`aceito = history_weight * conf_depth * conf_normal * conf_cor` --- **produto**,
e nao media. As tres confiancas respondem a mesma pergunta por caminhos
diferentes ("o pixel deste uv ainda e a mesma superficie do frame passado?") e
um "nao" isolado ja e resposta. Uma media deixaria duas confiancas altas
encobrirem a terceira, que e exatamente o caso da quina do painel contra o
para-brisa: mesma distancia, mesma cor, outra normal. O resultado seria borrao,
e borrao e o unico defeito que a acumulacao introduz e nao consegue desfazer.
O produto esta espelhado em C++ como `rtgi_history_alpha`, que e onde vira
teste.

Antes das tres confiancas vem o clamp de vizinhanca 3x3: a historia so pode
viver dentro da faixa que os vizinhos do frame atual ja ocupam. E o termo livre
de escala, e por isso o que carrega o peso num buffer escuro.

### 2. Rotacao de raios por frame

Acumular so ajuda se os frames trouxerem amostras **novas e bem distribuidas**.
Tres mudancas em `resolve_ray` e `ray_random`:

- **o hash saiu do `sin`.** Era
  `frac(sin(dot(seed, ...)) * 43758.5453)` com o frame somado dentro do seed. O
  argumento do `sin` cresce com o frame e com a resolucao, e em fp32 o `sin` de
  argumento grande perde exatamente os bits baixos que o `frac` usa. O hash ia
  empobrecendo ao longo dos minutos em que a acumulacao deveria estar somando
  amostras novas -- o pior momento possivel. Agora e PCG em inteiro: exato, e
  com a mesma qualidade no frame 1 e no frame 60000;
- **angulo dourado por frame.** O azimute ganha `frac(frame * 0.38196601)`.
  Frames sucessivos intercalam azimutes em vez de sortear independentes, e `N`
  raios ao longo de `K` frames cobrem o hemisferio muito melhor que `N*K`
  direcoes aleatorias;
- **estratificacao dentro do frame.** `random.x` passa a ser
  `(index + jitter) / ray_count`, o que reduz a variancia por frame de graca.

## O limite: nao ha reprojecao

O plugin roda no `Present` e nao tem as matrizes de camera nem motion vectors.
A historia e lida no **mesmo uv**, como o resolve temporal de 0.10.0 ja faz.

Normalmente isso seria uma limitacao seria. Aqui nao e, e vale entender por
que: **o interior da cabine e estavel em espaco de tela por construcao**.
Painel, volante, bancos e portas nao se movem em relacao a camera enquanto o
caminhao anda, entao para eles o mesmo-uv nao e uma aproximacao -- e a
reprojecao certa. E o interior e justamente o alvo desde a 0.13.2.

O que se move e a cena vista pelo para-brisa. Ali as rejeicoes derrubam a
historia em vez de borra-la, e o ruido volta ao nivel da 0.13.2.1. Em curva o
exterior inteiro volta. **Isso e o desenho funcionando, nao regressao.**

Motion vectors so chegariam com as matrizes de camera, que exigiriam ler
constant buffers do jogo -- outro projeto, e nao um ajuste deste modulo.

## A recalibracao de escala

`gi_intensity` sobe de `0.15` para `0.6`, e essa e a metade da versao que se ve.

A leitura das capturas de 30/08 (oito imagens com a 0.13.2.1, cabine e patio
de posto) nao mostrou diferenca nenhuma no interior. A conta explica sem
precisar de A/B: `sky_ambient` e `gi_intensity` sao os dois multiplicadores e
se empilham. O teto do que um raio escapado pode somar era
`0,25 x 0,15 = 0,0375` linear, e o raio tipico no painel fica bem abaixo do
teto -- para um hemisferio cosseno em torno de uma normal apontada ao
motorista, `saturate(direction.y)` tem media perto de `0,21`, o que da
`0,25 x 0,21 x 0,15 ~= 0,008` linear.

Sobre um plastico de painel em ~0,03 linear, isso leva a 0,038. Em sRGB: de
0,20 para 0,22, ou seja **cerca de 5 niveis em 255**. O conserto da 0.13.2.1
existia e estava correto; era invisivel. Com `0.6` o mesmo painel vai de ~49
para ~68 em 255.

`color_rejection` cai de `0.15` para `0.05` na mesma passada. O `0.15` veio do
resolve temporal, que opera sobre a imagem final; o buffer de GI e escuro
(valores na casa de 0,05) e um limiar absoluto de 0,15 ali nunca dispararia --
aceitaria historia sempre, que e ghosting.

## O que a 0.13.3 nao faz

- **denoiser espacial.** A acumulacao reduz variancia no tempo. Pixels vizinhos
  continuam independentes, e em movimento -- quando a historia e rejeitada --
  o ruido espacial aparece cru. E a 0.13.4;
- **preview do buffer acumulado.** `debug=temporal_gi` continua caindo no mesmo
  ramo de `raw_gi`. O preview do modo 6 re-executa a marcha em resolucao cheia
  e nao tem historia; mostrar o acumulado exige um passe de exibicao da meia
  resolucao, que vem junto com o visualizador de que a 0.13.4 precisa. **A
  validacao desta versao e a imagem final**, e por isso a recalibracao de
  escala entrou junto: sem ela nao havia o que olhar;
- **mascara de HUD.** A cor de cena continua sendo o backbuffer no `Present`,
  ja com interface. Depende da prova de composicao da branch
  `fsr-0.7.2-tiles`.

## Verificacao feita

- `build.sh` limpo com `-Wall -Wextra -Werror`; `validate.sh` verde;
  `rtgi_config_test` passa;
- **bytecode**: os cinco entry points aprovados de `photorealism.hlsl`,
  `ssao.hlsl`, `temporal.hlsl` e `depth-preview.hlsl` sairam **byte-identicos**
  ao HEAD anterior, 2367 linhas de disassembly iguais dos dois lados.
  `PSRtgi` foi de 682 para 722 linhas (hash inteiro, estratificacao e
  rotacao). `PSRtgiTemporal` tem 426 linhas. `PSRtgiCompose` mudou **uma
  linha**: `dcl_constantBuffer cb0[5]` virou `cb0[7]`, consequencia direta dos
  cinco campos novos no cbuffer compartilhado -- nenhuma instrucao mudou;
- **cada guarda nova provada falhando com a mensagem certa**, injetando a
  regressao numa copia da arvore. Tres achados nesse processo, todos
  corrigidos:
  1. o **hash SHA256 do cfg rodava antes de todas as guardas por chave**, entao
     nenhuma delas chegava a falar: qualquer edicao batia no hash e saia com
     "Configuracao consolidada foi alterada", que nao diz o que quebrou. O
     hash passou para depois das guardas nomeadas. Isso vale retroativamente
     para a guarda de `sky_ambient` da 0.13.2.1, que tambem estava muda;
  2. a guarda de "parametro zerado" estava **atras** dos pinos de valor exato,
     e portanto inalcancavel -- zerar um parametro falhava o pino primeiro. A
     ordem foi invertida: zerar e a regressao que merece explicacao;
  3. duas guardas novas eram `grep` nus sob `set -e`, que derrubam a validacao
     **sem mensagem**. Viraram guardas nomeadas. Uma terceira checava so o nome
     `rtgi_history_alpha`, que a declaracao `using` do teste ja satisfazia com
     zero cobertura; passou a checar a asserta em si.

## Como validar em jogo

Page Up faz o A/B.

- **cabine parada, de dia**: painel visivelmente mais claro que com o RTGI
  desligado, e **estavel** -- sem o formigamento da 0.13.2.1. E o caso em que o
  mesmo-uv e exato, entao e aqui que a acumulacao tem que aparecer inteira;
- **dirigindo em reta**: interior estavel; a cena pelo para-brisa pode granular
  mais;
- **fazendo curva**: interior estavel, exterior de volta ao ruido da 0.13.2.1.
  Esperado;
- **sinal de que passou do ponto**: rastro atras do que se move na tela. O
  recuo e `color_rejection` para baixo, ou `history_weight` de `0.90` para
  `0.80`. Os dois sao cfg, sem recompilar;
- **se o exterior lavar**: `gi_intensity` alto demais. Tambem cfg.
