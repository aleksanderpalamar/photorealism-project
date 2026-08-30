# Curva de tom 0.14.0 — e por que o RTGI para aqui

Estado: entregue. Muda a direcao da 0.13.x. O RTGI fica parado e desligado.

## O que motivou

O usuario mostrou cinco capturas do ATS com um shader de terceiros e pediu que
o plugin chegue naquele visual. Em vez de julgar no olho -- que foi como toda a
serie 0.13.x foi calibrada, e foi assim que um efeito de cinco niveis em 255
sobreviveu tres versoes -- os histogramas foram medidos.

| | p1 | p5 | mediana | canal mais alto |
|---|---|---|---|---|
| Referencia (4 imagens) | **8–11** | 9–12 | 11–40 | **G** |
| Plugin 0.13.3 (3 imagens) | **0** | 0–35 | 47–70 | **B** |

A medida esta reproduzivel em `tools/grade_report.py`.

## As tres leituras

**1. A referencia nunca chega ao preto.** O 1% mais escuro das quatro fica em
8–11 de 255. Nada e esmagado. O plugin batia em 0 nas tres, porque
`photorealism.hlsl` terminava em `saturate()` sem toe nenhum. E por isso que o
painel do plugin virava massa preta enquanto o da referencia -- **mais escuro
na mediana** -- deixava ler cada manometro.

**2. A referencia e mais escura, nao mais clara.** Mediana 11–40 contra 47–70.
A 0.13.2.1 e a 0.13.3 foram gastas somando luz ambiente para clarear a cabine,
e o alvo tem a cabine mais escura que a nossa. O problema nunca foi falta de
luz.

**3. O balanco de cor estava invertido.** A referencia puxa verde nas quatro; o
plugin puxa azul nas tres. `apply_temperature` so trocava R contra B e **nunca
tocava em G**: o eixo verde-magenta nao existia, e o alvo era inalcancavel por
qualquer combinacao dos valores existentes.

## A conta que fecha

Um lift linear de `0.0027` leva o preto a `0.0027 * 12.92 = 0.0349` em sRGB, ou
**8,9 em 255** -- exatamente a faixa medida. O alvo nao e vago; e um parametro.
`tests/tone_curve_test.cpp` guarda esse numero.

## O que entrou

- **`apply_black_lift`** — `lift + (1 - lift) * color`, em linear, e a **ultima
  coisa antes do encode**, depois da vignette. Antes dela os cantos
  escureceriam abaixo do piso e o piso deixaria de ser piso. `validate.sh`
  guarda essa ordem por numero de linha;
- **`apply_highlight_rolloff`** — acima do joelho os altos comprimem
  assintoticamente para 1 em vez de bater nele. Com forca `0.35` o joelho fica
  em 0,825 e o topo realista da cena (~1,06 depois de exposicao e contraste)
  sai em codigo 250 em vez de 255, que e onde a gradacao aparece;
- **eixo de tint** no balanco de branco, escalando G contra R e B com
  compensacao de metade do ganho, para mudar cor sem mudar brilho;
- **`blacks` da base de `-0.01` para `0.05`** — somado aos dois deltas ele valia
  `-0.06` e empurrava os pretos para baixo, contra o alvo. Agora soma zero e o
  piso fica por conta de `black_lift`;
- **`tools/grade_report.py`** — a medida vira ferramenta do repositorio, para a
  proxima rodada nao voltar ao olho.

Deliberadamente **nao** mudaram: exposicao, contraste, saturacao e vibrance. A
0.13.2 e a 0.13.3 moveram varias coisas de uma vez e nenhum A/B ficou
interpretavel. Esta versao move o que a medicao pede, e so.

Uma correcao de leitura: o plano inicial dizia que `contrast=0.98` reduz
contraste. Isso e o valor da camada base; o efetivo e a soma das tres camadas,
`0.98 + 0.08 + 0.01 = 1.07`. O contraste ja estava acima de 1 e nao precisava
de ajuste.

## O RTGI para aqui

Nenhuma das cinco referencias mostra um efeito que exija tracado de raios: a
luz de preenchimento da cabine e uniforme e **sem sangramento de cor** -- nao
ha verde da grama no painel nem vermelho do caminhao a frente -- e o brilho dos
mostradores ao anoitecer nao ilumina nada em volta. E AO, curva de tom e
grading.

O modulo fica desligado (`enabled=false`, como ja estava). Nao esta descartado;
esta na direcao errada para este alvo, e hoje empurra contra ele em dois eixos:
soma ruido onde a referencia e limpa, e levanta os meios-tons que precisam
descer.

`color_rejection` volta de `0.05` para `0.5`. O `0.05` da 0.13.3 era erro meu:
o termo compara o frame atual com o historico, e num buffer de GI a diferenca
entre os dois **e o ruido que a acumulacao existe para eliminar**. Com o corte
em 0,05 a historia era descartada todo frame, justamente nas superficies
escuras que mais precisavam dela -- que e por que o granulado continuou visivel
nas capturas da 0.13.3.

## Pendente: a faixa escura -- FECHADA na 0.16.0

> Resolvida, e nao como este documento previa: era o **proprio RTGI**. Testado
> em jogo na 0.16.0, sem o modulo a faixa nao existe -- a linha era a fronteira
> entre a regiao que recebia GI somado e a que nao recebia. O descarte de
> `PSRtgiCompose` abaixo confunde "so soma" com "nao pode criar aresta": um
> passe que soma **em parte da tela** desenha um contorno igual. O texto
> original fica como estava, porque o erro e o registro.


Linha horizontal nitida, largura inteira, a ~84% da altura, tudo abaixo mais
escuro. Aparece nas sete capturas da 0.13.3 e tambem nas da 0.13.2.1.

Ja descartado por leitura de codigo: `PSRtgiCompose` **so soma**, entao nao
pode escurecer; e a vignette e radial e suave, com peso 0,03.

Hipotese principal: o SSAO. `ssao.hlsl` faz
`distance_fade = 1 - smoothstep(30, 70, center_distance)`, e em chao plano
distancia constante e altura de tela constante -- a fronteira e horizontal por
construcao. Reforca: a calibracao de `radius`/`intensity`/`fade` foi aprovada
nas versoes 0.7.0 a 0.9.1 **sobre uma cascata de sombra**, e a 0.13.0 corrigiu
a elegibilidade do depth. O SSAO passou a rodar sobre o depth certo com numeros
afinados para outro buffer. Ressalva: `smoothstep` da gradiente, nao aresta.

**Nao foi resolvido nesta versao porque depende de teste em jogo**, e nao de
leitura de codigo. A ordem importa: testar a faixa isolada primeiro, so depois
a curva. Julgar as duas juntas foi o erro que fez a 0.13.2 e a 0.13.3 passarem
sem nada conclusivo.

## Como validar

Medido, e nao olhado. `./tools/grade_report.py <referencias> -- <capturas>`:

- **p1 entre 6 e 12** (era 0). E o unico numero que sozinho separa os dois
  visuais;
- **G como canal mais alto** de dia (era B);
- **mediana caindo** em relacao a 0.13.3, nao subindo;
- **topo% baixo**, ou seja o ceu rolando em vez de cortar.

Em jogo, na ordem:
1. `[module.ssao.0.7.0] enabled=false` — a faixa some? Se sim, o Estagio 1 da
   proxima versao e recalibrar o SSAO sobre o depth certo;
2. se persistir, `[base.0.1.2] enabled=false` isola o grading;
3. so entao julgar a curva.
