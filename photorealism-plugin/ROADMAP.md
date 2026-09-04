# Roadmap

O modulo FSR foi removido na 0.15.0 e o de tracado de raios na 0.16.0, junto
com o `FSR_ROADMAP.md` que este cabecalho citava. O que sobrou e o que o alvo
visual medido usa: curva de tom, coloracao, iluminacao, TAA/AA nativo, SSAO e,
desde a 0.17.0, bloom. As secoes historicas abaixo ficam como registro.

## 0.1.x a 0.3.x - Fundacao e calibracao visual

- hook do `IDXGISwapChain::Present` em Direct3D 11;
- passe de cor, tons, contraste local e nitidez;
- recarga em tempo real e comparacao A/B;
- calibracoes de base, visual geral e chuva/tempo nublado;
- validacao no ETS2 1.60 sob Proton/DXVK.

## 0.4.x - Arquitetura dual e calibracao cumulativa

- `dinput8.dll` dedicado ao bootstrap e DirectInput;
- `dxgi.dll` dedicado ao proxy DXGI e ao nucleo grafico;
- fabricas `CreateDXGIFactory*` encaminhadas ao DXGI do sistema;
- camadas historicas somadas sem sobrescrever calibracoes anteriores;
- resultado visual efetivo da 0.3.0 preservado durante a migracao.

## 0.5.0 - Robustez do ciclo de swap chain

- hook e tratamento de `ResizeBuffers`;
- deteccao de recriacao/redimensionamento do swap chain;
- invalidacao seletiva e reconstrucao automatica dos recursos de frame;
- sincronizacao entre apresentacao e redimensionamento.

## 0.5.1 - Linha de base de desempenho

- queries D3D11 de timestamp sem bloqueio;
- anel de amostras compativel com entrega atrasada do DXVK;
- media, minimo, pico e descartes registrados a cada dez segundos;
- continuidade do passe quando a telemetria estiver indisponivel.

## 0.5.x - Robustez DXGI complementar

- compatibilidade com formatos adicionais de backbuffer;
- fallback de hook caso a vtable compartilhada nao funcione no DXVK usado.

## 0.6.0 - Descoberta de profundidade

- hooks da familia `OMSetRenderTargets` no contexto D3D11;
- catalogo temporario de depth-stencil views;
- classificacao de candidatos em resolucao nativa;
- verificacao de formato, MSAA e permissao de leitura por shader;
- nenhuma aplicacao espacial antes de validar o recurso correto.

## 0.6.1 - Descoberta com escala interna

- agrupamento das views pela textura de profundidade real;
- suporte a resolucoes internas diferentes do backbuffer;
- classificacao por area, proporcao de tela e frequencia de uso;
- retencao dos candidatos relevantes quando o catalogo atinge o limite;
- relatorio separado de grupos de resolucao e recursos individuais.

## 0.6.2 - Captura nao destrutiva e preview

- retencao segura do candidato principal depois da descoberta;
- copia para textura typeless legivel por shader sem modificar o jogo;
- visualizacoes raw, forward-Z e reversed-Z em escala de cinza;
- fallback automatico para o passe visual em qualquer falha;
- confirmacao visual da geometria antes de integrar efeitos espaciais.

## 0.6.3 - Linearizacao reversed-Z

- consolidacao da orientacao reversed-Z confirmada pelas imagens;
- realce logaritmico da profundidade reversed-Z;
- conversao para distancia pelo modelo de plano distante infinito;
- parametros diagnosticos separados da calibracao visual;
- validacao da distribuicao de distancia antes do primeiro SSAO.

## 0.6.4 - Normais reconstruidas

- alcance do preview linear reduzido para destacar os primeiros 50 metros;
- reconstrucao aproximada da posicao em espaco de camera;
- derivadas escolhidas pelo menor salto de profundidade nas silhuetas;
- visualizacao RGB das normais sem alterar o shader visual aprovado;
- validacao estrutural final antes de experimentar SSAO.

## 0.7.x - Iluminacao espacial

- primeiro SSAO baseado na profundidade e nas normais validadas;
- composicao em passe separado, preservando o shader visual aprovado;
- monitor de atividade do depth e recuperacao automatica em transicoes de
  menu sem `ResizeBuffers`;
- atividade confirmada por binding ou limpeza real do depth, com tolerancia
  curta para continuidade entre frames;
- suspensao preventiva do SSAO quando a profundidade nao pertence ao frame
  atual, preservando o tratamento photorealista sem manchas;
- oito amostras, raio curto, rejeicao de silhuetas e fade por distancia;
- mascara de visibilidade para calibracao visual e deteccao de halos;
- refinamento separado para interior e exterior;
- preservacao de farois, flares e chuva em cenas noturnas.

## 0.8.x - Refinamento SSAO photorealista

- dois aneis simetricos e 16 amostras para contatos menos direcionais;
- intensidade media preservada em relacao ao SSAO aprovado;
- protecao adaptativa de altas luzes para farois, flares, chuva e reflexos;
- configuracao cumulativa independente, com retorno ao SSAO 0.7.0;
- refinamento separado para interior e exterior depois da validacao.

## 0.9.0 - Refinamento SSAO interior e exterior

- perfil exterior 0.8.0 preservado para a geometria alem do campo proximo;
- perfil de cabine/interior de raio curto, menor intensidade e maior rejeicao
  de bordas;
- transicao continua baseada na distancia linear do depth, sem etiqueta ou
  alteracao semantica do ETS2;
- configuracao independente, recarregavel por `End` e desativavel sem perder
  o SSAO exterior consolidado.

## 0.9.1 - Estabilidade do depth em ETS2 e ATS

- ranking orientado pela frequencia real de bindings, com suporte a escala
  interna assimetrica;
- retencao temporaria de todos os candidatos observados e consolidacao somente
  do vencedor final;
- confianca minima para rejeitar depths de menu e interface;
- testes de regressao com os candidatos registrados nos logs dos dois jogos;
- nenhuma alteracao nos shaders ou na calibracao visual da 0.9.0.

## 0.10.0 - Primeira integracao espacial e temporal

- resolve temporal posterior e compativel com o TAA nativo do jogo;
- historicos nao destrutivos de cor e profundidade;
- clamp por vizinhanca e rejeicao por cor/depth para limitar ghosting;
- invalidacao automatica nas mesmas transicoes protegidas pelo SSAO;
- primeira calibracao voltada a cintilacao de vegetacao e detalhes distantes.

## 0.10.1 - Inicializacao espacial e temporal automatica

- descoberta permanece ativa por ciclos enquanto o jogo estiver no menu;
- entrada no mundo 3D consolida automaticamente um depth seguro;
- escala interna e taxa de bindings separam a cena principal da interface;
- SSAO e resolve temporal iniciam sem qualquer acionamento de `End`;
- atalhos continuam disponiveis somente para comparacao e diagnostico;
- nenhuma alteracao na calibracao visual consolidada da 0.10.0.

## 0.11.0 - AA espacial/temporal proprio antes da interface

- desativacao automatica e reversivel dos cvars AA/TAA nativos somente em
  ETS2/ATS, com backup, escrita atomica e log detected/applied;
- selecao comprovada do scene-color no passe de composicao anterior a UI;
- AA espacial edge-aware, historico ping-pong, clamp/rejeicao e busca local
  de correspondencia temporal, sem depender de tecla;
- RCAS conservador depois do temporal; EASU permanece condicional a upscale
  seguro e fonte realmente menor;
- telemetria separada antes de qualquer afirmacao de superioridade visual;
- F12 Steam unico e a pilha visual consolidada permanecem preservados.

## Etapa final - Refinamento temporal

- validar reducao de cintilacao em movimento nos dois jogos;
- calibrar separadamente estabilidade e rejeicao de historico, caso necessario;
- avaliar acesso seguro a jitter ou vetores de movimento antes de evoluir o AA
  proprio para reprojecao completa.

Cada etapa so avancara apos comparacao A/B, teste de chuva, amanhecer,
entardecer, noite e verificacao de frame time.

## 0.12.0 - Screen-Space Ray-Traced Global Illumination (SSRTGI)

Luz indireta em screen-space, de curto/medio alcance (0.5 m a 15 m), sobre
cabine, caminhao, asfalto, paredes, postos, edificios e vegetacao proxima. O
nome e deliberado: screen-space, nao hardware ray tracing. DXR/D3D12 sobre os
RT cores da RX 6600 fica explicitamente fora do escopo.

O GI roda **antes** do grading. O diagrama original da tecnica pedia
`SSAO -> GI -> Temporal -> grading`, mas a cadeia real do plugin sempre foi
`grading -> SSAO -> temporal`: inverte-la invalidaria a calibracao consolidada
da base 0.1.2 + 0.2.0 + 0.3.0, os limiares de highlight do SSAO e o
`color_rejection` do temporal. Pondo o GI antes do grading, o motivo declarado
-- o grading alcancar tanto a luz direta quanto a indireta -- e atendido sem
custo de recalibracao:

```
scene color -> SSRTGI -> grading -> SSAO -> temporal -> backbuffer
```

As fases:

- **0.12.0 (entregue)** consolidacao da matematica depth/view-space numa fonte
  unica, configuracao, buffers em meia resolucao e o passe inerte; nenhum raio
  e tracado;
- **0.12.1 (entregue)** ray march de raio unico em screen-space, com acerto por
  espessura, contribuicao de ceu no miss e confianca separando "vazio" de
  "desconhecido"; o resultado preenche o buffer mas ainda nao e composto;
- **0.13.2 (entregue)** GI difusa de quatro raios, amostragem cosine-weighted,
  rejeicao de firefly por raio e composicao com `gi_intensity`; e a versao em
  que o RTGI passou a alterar a imagem do jogo. Inclui a **marcha geometrica**,
  descrita abaixo, sem a qual a composicao nao alcancava a cabine. Detalhe em
  `references/rtgi-composition-0.13.2.md`;
- **0.13.2.1 (entregue)** o raio que escapa sem acertar nada deixa de devolver
  preto. Os tres desfechos de nao-acerto de `march_ray` passam a compartilhar
  `ambient_escape`, e `sky_ambient` sai de `0.0`. E o que faz o RTGI alcancar
  o interior da cabine; ver **O escape do raio**, abaixo;
- **0.13.3 (entregue)** acumulacao temporal com rotacao de raios por frame,
  somando `normal_rejection` a rejeicao de depth e cor que ja existia. Traz
  junto a recalibracao de `gi_intensity`, sem a qual a versao nao teria como
  ser avaliada; ver **A acumulacao e a escala**, abaixo. Detalhe em
  `references/rtgi-temporal-0.13.3.md`;
**As tres fases seguintes de RTGI foram removidas do roteiro na 0.16.0**,
junto com o modulo. A razao esta medida em **A medicao que parou o RTGI**,
abaixo, e o historico fica nos cinco `references/rtgi-*.md`.

### A remocao do FSR

A 0.15.0 apagou o modulo de AA/FSR auxiliar inteiro: 5.833 linhas, 26
arquivos, um DLL do pacote. A decisao foi do usuario, e a evidencia estava no
proprio log ha versoes -- `replacement=0 dispatch=0`. O modulo nunca
substituiu um draw nem despachou um passe.

O ganho maior nao esta nas linhas apagadas. **Oito hooks de vtable existiam so
para alimenta-lo** -- `PSSetShaderResources`, `RSSetState`, `RSSetViewports`,
`RSSetScissorRects` e os quatro `Draw*` -- e o ETS2 emite milhares de draws por
frame. Cada um pagava indirecao, load atomico e branch por um modulo inerte.

Os hooks de `OMSetRenderTargets` e da variante com UAVs **ficaram**: a
descoberta de depth vive neles, e sem ela SSAO, resolve temporal e RTGI ficam
sem fonte. `native_aa` tambem ficou -- ele administra o `r_aa` do jogo, que e o
TAA, e nunca teve relacao com FSR alem do nome do pacote.

Duas guardas em `validate.sh` impedem o retorno: uma para o codigo, outra para
os oito hooks. E a primeira versao dessa guarda usava `easu|rcas` no padrao e
derrubava `grade_report.py`, que contem "m**easu**re" -- registrado aqui porque
o mesmo tipo de colisao vai reaparecer na proxima guarda por substring.

### A medicao que parou o RTGI

A 0.14.0 mediu os histogramas de cinco capturas do ATS com um shader de
terceiros -- o alvo visual pedido -- contra a saida da 0.13.3:

| | p1 | mediana | canal mais alto |
|---|---|---|---|
| Referencia (4 imagens) | **8–11** | 11–40 | **G** |
| Plugin 0.13.3 (3 imagens) | **0** | 47–70 | **B** |

Tres conclusoes, e a terceira e a que para o modulo:

1. **a referencia nunca chega ao preto**, e o plugin batia em 0 nas tres
   capturas. Era o `saturate()` sem toe, nao falta de luz;
2. **a referencia e mais escura na mediana**, nao mais clara. A 0.13.2.1 e a
   0.13.3 foram gastas somando ambiente para clarear a cabine, na direcao
   oposta;
3. **nenhuma das cinco referencias mostra efeito que exija tracado de raios.**
   A luz de preenchimento da cabine e uniforme e sem sangramento de cor -- nao
   ha verde da grama no painel nem vermelho do caminhao a frente -- e o brilho
   dos mostradores ao anoitecer nao ilumina nada em volta. E AO, curva de tom e
   grading.

O RTGI **nao esta descartado**. Esta na direcao errada para este alvo, e hoje
empurra contra ele em dois eixos: soma ruido onde a referencia e limpa, e
levanta os meios-tons que precisam descer. Fica desligado
(`[module.rtgi.0.12.0] enabled=false`) e as fases seguintes ficam pausadas.

A licao de processo, que vale para tudo daqui em diante: **toda a serie 0.13.x
foi calibrada no olho**, e foi assim que um efeito de cinco niveis em 255
sobreviveu tres versoes sem que ninguem percebesse que era invisivel.
`tools/grade_report.py` existe para que isso nao se repita.

### A faixa escura -- RESOLVIDA na 0.16.0, era o RTGI

Linha horizontal nitida, largura inteira, a ~84% da altura, tudo abaixo mais
escuro. Aparecia nas capturas da 0.13.2.1 e da 0.13.3.

**Era o proprio RTGI.** Testado em jogo na 0.16.0: com o modulo removido a
faixa nao existe. O usuario esclareceu que ela so aparecia com o RTGI ligado --
a parte inferior da tela era onde o tracado nao alcancava, e a linha era a
fronteira entre a regiao que recebia `indirect * gi_intensity` somado e a que
nao recebia nada.

**A leitura de codigo que a descartou estava errada, e o erro tem forma.**
"`PSRtgiCompose` so soma, entao nao pode escurecer" trata a soma como se fosse
absoluta, quando o que se ve na tela e contraste: um passe que **so soma, mas
nao em toda parte**, desenha uma aresta tao visivel quanto um que subtrai. O
lado escuro nao foi escurecido -- foi o unico que nao foi clareado.

**E a hipotese do SSAO foi construida sem checar a evidencia mais barata.** A
faixa aparecia exatamente nas versoes em que o RTGI executava, e havia 16
capturas da 0.14.0 -- ja com `enabled=false` -- que teriam fechado a questao em
um olhar. Em vez disso a investigacao foi para dentro do `ssao.hlsl` procurar
um mecanismo. **O intervalo de versoes em que um sintoma aparece e um dado, e
costuma chegar antes de qualquer leitura de shader.**

### Marcha geometrica

Os parametros do RTGI vieram do documento da tecnica com escala externa em
mente -- asfalto, paredes, postos, edificios. Na cabine eles nao alcancavam a
geometria nem em principio:

```
range_min=0.5   range_max=15.0   max_steps=12
passo = (15.0 - 0.5) / 12 = 1,21 m

amostras em  1,71  2,92  4,13  5,33 ... 15,0 m
```

`travelled` comecava em `range_min` e o laco somava o passo **antes** da
primeira amostra, entao nada era amostrado entre 0,5 e 1,71 m. A cabine inteira
vive nessa faixa: banco a ~0,5 m, painel e GPS a ~0,7 m, para-brisa a ~1 m. Um
raio saindo do painel pulava a cabine e ia amostrar o asfalto.

A saida considerada primeiro foi espelhar `[module.ssao_interior.0.9.0]`: um
perfil de curto alcance misturado por distancia de camera. Foi **preterida** em
favor de trocar a distribuicao dos passos por progressao geometrica:

```
razao = (range_max / range_min) ^ (1 / max_steps)

com 0.10 a 15.0 em 12 passos, razao = 1,5182:
  0,15  0,23  0,35  0,53  0,81  1,22  1,86  2,82  4,29  6,51  9,88  15,0
         ^------ seis dentro da cabine ------^
```

Um perfil unico cobre as duas escalas, sem seccao nova no cfg e sem a heuristica
de "isto e cabine?" -- que teria o efeito colateral de fazer um carro a 3 m na
estrada contar como interior e perder o alcance longo. `hit_thickness` virou
teto em vez de valor fixo, porque a ambiguidade que a marcha introduz e o
proprio comprimento do passo.

A cobertura do interior e teste, nao aritmetica de comentario:
`rtgi_samples_within(0.10f, 15.0f, 12, 1.5f) >= 4` falha com `range_min=0.5`.

### O escape do raio

A marcha geometrica era necessaria e nao era suficiente. Com ela entregue, o
teste em jogo da 0.13.2 mostrou a cabine ainda preta -- inclusive dentro de um
tunel de concreto branco iluminado em volta inteira, que e a geometria mais
favoravel a GI que o jogo oferece. E preta **sem granulado**, enquanto do lado
de fora havia granulado. Ruido ausente onde deveria haver ruido nao e denoise
faltando; e sinal ausente.

`march_ray` tem quatro desfechos, e so um e acerto real:

| desfecho | devolvia ate a 0.13.2 |
|---|---|
| acerto numa superficie | cor da cena |
| saiu da tela | `sky_ambient * dir.y` |
| ceu de verdade (`raw_depth == 0`) | `sky_ambient * dir.y` |
| passos esgotados, ou plano proximo cruzado | **zero** |

E `sky_ambient` valia `0.0` no cfg. Somando as duas coisas, **os quatro
desfechos devolviam preto menos o acerto real** -- o shader respondia breu a
todo "nao sei".

Isso custa uma cabine inteira porque o acerto real e inalcancavel ali.
`reconstruct_view_normal` termina com `if (normal.z > 0.0) normal = -normal;`:
toda normal visivel aponta para a camera. O hemisferio de amostragem do painel
e entao o cone **entre o painel e o olho do motorista**, que e ar vazio. Os
raios tipicos andam para tras e caem no `break` do plano proximo por volta da
sexta amostra; os rasantes sobem em direcao ao para-brisa, ficam na tela mas
voam a frente da estrada -- que esta a dezenas de metros, com `delta` negativo
nos doze passos -- e esgotam. Quatro raios devolvendo exatamente `0.0` tem
media exatamente `0.0`: preto liso.

A correcao e um termo unico, `ambient_escape(direction)`, compartilhado pelos
tres desfechos de nao-acerto, e `sky_ambient=0.25`. O valor nao e a radiancia
de um ceu: e a de uma direcao **desconhecida**, e em jogo a maioria delas esta
parcialmente ocluida. A confianca continua em zero nesses desfechos, que e do
que a rejeicao temporal da 0.13.3 precisa para confiar menos neles.

**O que isso nao faz.** Da a cabine um piso de ambiente modulado pela direcao
do raio -- a penumbra. Nao da color bleeding do exterior para dentro: a estrada
esta *atras* do painel em view-space, fora do hemisferio dele, e nenhum ajuste
de parametro alcanca isso em screen-space puro. Superficie virada para a camera
so enxerga o que esta **ao lado dela, em profundidade parecida**.

### A acumulacao e a escala

A 0.13.3 tem duas metades, e a segunda entrou porque a primeira nao teria como
ser avaliada sem ela.

**A acumulacao.** Quatro raios por pixel e ruido, e ate a 0.13.2.1 esse ruido
ia inteiro para a tela. `PSRtgiTemporal` soma o frame anterior sob tres
rejeicoes **multiplicadas** -- profundidade, normal e cor. Produto, e nao
media: as tres respondem a mesma pergunta por caminhos diferentes, e um "nao"
isolado ja e resposta. Media deixaria duas confiancas altas encobrirem a
terceira, que e exatamente a quina do painel contra o para-brisa -- mesma
distancia, mesma cor, outra normal -- e o resultado seria borrao.

**Nao ha reprojecao**, e isso e um limite medido, nao uma pendencia. Sem as
matrizes de camera a historia e lida no mesmo uv. Para o interior da cabine
isso e **exato**: painel, volante, bancos e portas nao se movem em relacao a
camera enquanto o caminhao anda. Para a cena vista pelo para-brisa a historia e
rejeitada em movimento, e em curva o exterior volta ao ruido da 0.13.2.1.
Motion vectors exigiriam ler constant buffers do jogo -- outro projeto.

**A escala.** As oito capturas com a 0.13.2.1 nao mostraram diferenca nenhuma
no interior, e a conta explica sem precisar de A/B: `sky_ambient` e
`gi_intensity` sao dois multiplicadores que se empilham. O teto do que um raio
escapado podia somar era `0,25 x 0,15 = 0,0375` linear, e o raio tipico no
painel fica bem abaixo do teto -- num hemisferio cosseno em torno de uma normal
apontada ao motorista, `saturate(direction.y)` tem media perto de `0,21`, o que
da `~0,008` linear. Sobre um plastico em ~0,03 linear isso e **cerca de 5
niveis em 255**.

O conserto da 0.13.2.1 existia e estava correto. Era invisivel. `gi_intensity`
subiu para `0.6`, e o mesmo painel passa de ~49 para ~68 em 255.

A licao vale para as proximas fases: **um efeito multiplicado por dois ganhos
pequenos em serie pode estar certo e nao aparecer**, e nesse caso a debug view
e a unica coisa que distingue "nao funciona" de "nao da para ver".

### Luz emissiva de painel e GPS

A cabine a noite e iluminada pelo painel, pelo radio e pela tela do GPS. Fazer
essas fontes contribuirem como luz indireta **nao exige codigo novo**: o ray
march ja amostra `scene_texture_` no ponto de acerto, e nao pergunta se aquele
texel e emissivo. Um raio saindo do volante que atinge a tela do GPS ja pega a
cor dela. E o caso em que screen-space e mais forte, porque a fonte esta
visivel na tela por construcao -- ao contrario do sol, que quase nunca esta.

Quatro coisas precisam existir antes de funcionar:

1. ~~alcance~~ **resolvido na 0.13.2.** O GPS fica a ~0,7 m do olho, dentro da
   faixa cega de 0,5 a 1,71 m que a marcha geometrica eliminou. Hoje ha seis
   amostras entre 0,15 e 1,22 m. E o limite descoberto na 0.13.2.1 nao atinge
   este caso: GPS e painel estao **lado a lado em profundidade parecida**, que
   e exatamente a geometria que o screen-space alcanca -- diferente da estrada
   vista pelo para-brisa, que esta atras do painel e fora do hemisferio dele;
2. **`max_indirect_luma` calibrado para a noite.** Existe e ja e aplicado por
   raio desde a 0.13.2, mas o valor 4.0 veio do documento e nunca foi ajustado
   para tela clara contra cabine escura -- que e o caso extremo que ele existe
   para conter. Baixo demais o GPS nao contribui, alto demais ele estoura;
3. ~~acumulacao temporal (0.13.3)~~ **metade resolvida.** O GPS e uma fonte
   pequena, clara e de alta frequencia: com poucos raios, acertar ou nao vira
   cara-ou-coroa e o resultado cintila. A acumulacao da 0.13.3 resolve isso com
   a camera parada ou em movimento suave, que e a maior parte do tempo dentro
   da cabine -- e o interior e onde o mesmo-uv e exato. Falta o **denoise
   bilateral (0.13.4)** para o caso em que a historia e rejeitada;
4. **mascarar a HUD.** A cor de cena e uma copia do backbuffer no Present, ja
   com interface. De dia isso e uma limitacao conhecida; de noite piora, porque
   a HUD e proporcionalmente muito mais clara que a cabine escura e passaria a
   injetar luz que nao existe. Depende da prova de composicao, hoje em
   investigacao na branch `fsr-0.7.2-tiles`.

`InputNeedsSrgbDecode`, que entrou na 0.12.1, ja garante que o bounce e
calculado em espaco linear -- o que importa muito neste caso, porque sem ele
uma fonte clara contra fundo escuro sairia clara demais.

As fases do RTGI saltam de 0.12.1 para 0.13.2 porque **o numero e carimbo de
chegada, nao de agenda**. O documento original da tecnica numerou as sete fases
como 0.12.0 a 0.12.6 supondo que sairiam em sequencia, mas entre a 0.12.1 e a
fase seguinte entraram dois consertos nao planejados -- a politica de AA nativo
(0.12.2) e a elegibilidade do depth (0.13.0), mais o Page Down (0.13.1) -- e
cada um consumiu um numero. As promessas antigas passaram a colidir com pacotes
ja entregues e foram renumeradas.

A regra, daqui para frente: **quando um pacote sai, as versoes ainda nao
entregues deste arquivo sao renumeradas na mesma hora**, para que nenhuma
promessa aponte para um numero ja usado. `tools/validate.sh` verifica isso.

A partir da 0.13.3 o RTGI precisa ser executado uma unica vez por frame, antes
dos quatro draws de composicao ladrilhados do Prism3D -- substituir tile a tile
reproduziria o artefato de quadrantes corrigido na 0.11.2. Isso depende da
prova de composicao, em investigacao na branch `fsr-0.7.2-tiles`.

## 0.13.0 - Elegibilidade do depth de camera

Correcao da regra que rejeitava o depth de camera por construcao, e que mantinha
RTGI, SSAO e resolve temporal sem fonte. Detalhe em
`references/depth-eligibility-0.13.0.md`.

- **0.16.1 (proxima)** recalibrar SSAO sobre o depth certo. Se o depth de
  camera nunca foi usado, a calibracao aprovada nas versoes 0.7.0 a 0.9.1 foi
  feita sobre uma cascata de sombra, e `radius`, `intensity` e `fade` precisam
  de nova rodada A/B. **Desceu de prioridade na 0.16.0**: a faixa escura, que
  era a razao de ter subido, era o RTGI e nao o SSAO. Continua valendo por si
  -- calibracao afinada sobre um buffer, rodando sobre outro -- mas sem
  sintoma reportado atras dela;
- **0.19.0 (proxima)** adaptacao de cor por condicao, em cima do observador da
  0.18.0. A calibracao de hoje e a media de cinco condicoes diferentes, e um
  `tint` unico nao alcanca as cinco: e por isso que o valor efetivo de 0,50
  cai entre o alvo de dia claro e o de encoberto errando os dois. Precisa,
  nesta ordem: limiares medidos no ETS2 a partir das linhas `Cena 0.18.0:` do
  log; ancoras de `temperature`/`tint` por condicao; interpolacao **continua**
  entre elas, porque a margem entre condicoes e de so 1,5x sobre a dispersao
  interna e classe dura saltaria a cor ao virar a cabine; e suavizacao com
  **constante de tempo de 2 a 3 minutos** mais histerese. Cuidado central: o
  jogo ja renderiza a cor da hora. **Medido no ETS2 na 0.18.1**
  (`references/scene-baseline-ets2-0.18.1.md`, 36 amostras de jogo): o ceu R/B
  sobe +0,0152 por minuto sozinho, antes do grade, e a hora do dia explica 58%
  de toda a variacao de cor da sessao. Somar uma rampa de relogio por cima
  conta duas vezes. O alvo e o ajuste que **falta** em cada condicao, nao uma
  rampa artistica. Tres numeros ja saem medidos e substituem estimativa:
  a faixa do ceu R/B no ETS2 e 4,2x mais larga que a do ATS, entao **nenhum
  limiar do ATS serve**; o residuo depois da hora do dia tem desvio 0,069 e
  decorrelaciona em menos de um minuto, o que e a camera virando e nao mudanca
  de tempo, e e o que fixa a janela em 2-3 minutos; e 10% das amostras nao sao
  jogo (carregamento, fade, mapa), uma delas devolvendo o valor mais quente da
  sessao a partir de um quadro quase preto, entao a porta de jogo vem antes do
  detector. Falta o que so o usuario pode dar: **as linhas rotuladas pela
  condicao na tela**, sem as quais nao ha ancora de cor por condicao;
- **0.20.0** raios de sol. E o efeito que as referencias realmente
  mostram, e que a medicao do bloom revelou: estriados radiais saindo do sol
  atras da linha de arvores, projetados no teto escuro da cabine. Sao
  **direcionais**, e nenhuma piramide gaussiana produz aquilo. Reaproveita o
  bright-pass, a cadeia de reducao, a composicao aditiva e as guardas da
  0.17.0 -- falta um shader de blur radial e, o problema de verdade,
  descobrir a posicao do sol na tela sem dados do motor no `Present`.
  **Desceu de prioridade na 0.18.0**: cor errada em toda condicao pesa mais
  que um efeito ausente;
- **0.21.0 (condicional)** upgrade de bind flag via hook de `CreateTexture2D`,
  na tecnica do ReShade: promover o depth a typeless com
  `BIND_SHADER_RESOURCE`, sintetizando o descritor no `CreateDepthStencilView`.
  So entra se o `CopyResource` de um depth `DEPTH_STENCIL`-only falhar sob
  DXVK. Hoje nao ha evidencia de que falhe -- o plugin ja copia para textura
  propria e cria o SRV sobre a copia.
