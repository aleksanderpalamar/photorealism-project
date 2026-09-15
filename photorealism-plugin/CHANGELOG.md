# Changelog

## Pacote 0.24.0 - 2026-09-15

**Captura do quadro para analise.** Primeiro passo do grupo entre passes do jogo:
antes de mexer no G-buffer, no HDR ou no espelho, e preciso saber em que alvo cada
coisa esta. Nada muda na imagem. Detalhes em `references/captura-quadro-0.24.0.md`.

### O que muda

- botao **Capturar quadro para analise** na pagina inicial do menu. O quadro
  seguinte do jogo e gravado inteiro e o botao passa a mostrar "Captura salva: N
  alvos";
- em cada `OMSetRenderTargets` do jogo sao registrados todos os slots (ate 8
  alvos de cor e o depth), com tamanho, formato da textura, formato da view, mip e
  fatia;
- cada alvo e copiado quando o passe seguinte deixa de liga-lo: o arquivo guarda o
  que aquele passe escreveu. Um alvo religado mais tarde no quadro gera outra
  copia;
- os arquivos vao para `photorealism-plugin/captura-quadro-AAAAMMDD-HHMMSS/`: um
  DDS por copia, no formato original, e um `manifesto.json` com os binds e as
  copias. Limite de 160 copias e 1,5 GB; passando disso o manifesto marca
  `truncado`;
- a gravacao roda no Present e o jogo pausa enquanto os arquivos sao escritos;
- `tools/gbuffer_report.py <pasta>` gera `relatorio.md` e previews PNG de cada
  alvo, com estatisticas por canal e etiquetas: normal, possivel normal
  codificada, id de material, possivel velocidade, HDR, cor LDR, profundidade.

### Verificacao

- **GPU** (Wine + DXVK, "jogo" sintetico):
  - 4 alvos em MRT, depth `D32_FLOAT_S8X24`, view sRGB sobre textura typeless,
    `R11G11B10`, `R8G8`, mip 1 e fatia 4 de array;
  - os 9 arquivos gravados decodificam para os valores desenhados;
  - um alvo limpo de novo depois de liberado saiu com o valor do passe, e nao
    com o posterior.
- **Testes de host**: `frame_capture_logic_test` (agenda de um quadro, fim de
  passe, reabertura com a mesma identidade, cabecalho DDS, manifesto) e o
  autoteste da ferramenta sobre capturas sinteticas de resposta conhecida.
- **Guardas**, quebradas numa copia: so passes do jogo entram na captura, o
  quadro fecha no Present antes de o plugin desenhar, o botao existe, e a linha de
  log e o autoteste da ferramenta.
- **Menu**: rasterizado com o botao antes e depois do clique.

**Ainda nao rodou no jogo.**

## Pacote 0.23.3 - 2026-09-14

**As opcoes do menu que o pos-processo alcanca passam a mudar a imagem.** Na
0.23.2 so iluminacao, cores (menos quatro controles) e FSR mudavam algo no
jogo. Detalhes em `references/efeitos-0.23.3.md`.

### O que muda

- **Brancos**: virou ponto de branco nos claros, e nao mais soma de no maximo 6
  codigos. Medido na GPU: -0.13 leva 250 a 243 e 230 a 225; +0.13 leva 230 a 236;
- **Qualidade alta/media/baixa**: SSAO com 16/12/8 amostras (baixa tambem em meia
  resolucao) e bloom com ate 5/4/3 niveis;
- **Anti-aliasing**: Desligado desliga o resolve temporal; Temporal liga; Temporal
  nitido liga o resolve e uma nitidez RCAS 0.90 depois dele, fora do historico.
  DLAA e DLSS aparecem esmaecidos e nao podem ser escolhidos. O `taa=4` do cfg de
  referencia vale como Temporal nitido;
- **FXAA**: passe proprio antes do grade. Medido: 91 pixels de borda suavizados
  numa diagonal de teste, nenhum pixel longe da borda alterado;
- **SSAO**: `ssao_intensity` virou a forca da oclusao (ganho 2.2 sobre a base),
  e o slider agora aparece: visibilidade minima 0.69 em 1.0, 0.54 em 1.5 e 0.00 em
  4.0 no canto de teste. Preset suave/medio/forte muda raio, vies e curva;
  detalhe alto/medio/baixo muda as amostras; "SSAO em baixa resolucao" calcula a
  oclusao em meia resolucao e compoe com upsample bilateral pelo depth;
- **Luz de interior**: clareia so o que esta perto da camera (1.5 a 4 m, a cabine),
  proporcional a luminancia externa. Medido: 128 vai a 139 com a forca 0.17 e a
  222 com 2.0; fora da cabine e ceu ficam iguais;
- o log ganha a linha `Efeitos 0.23.3` na leitura do cfg e a cada mudanca no menu.

### O que nao mudou

Exposicao noturna ja funcionava e so age a noite: a sessao testada era de dia
(`Condicao 0.19.0: noite=0.000`). Pre-exposicao, pre-contraste, motion blur,
espelhos, SSS, albedo e normais da estrada dependem da captura da 0.24.0; folhas,
grama e chuva, dos shaders do jogo na 0.25.x.

### Verificacao

- **GPU** (Wine + DXVK, com o `d3dcompiler_47.dll` do ETS2): os cinco shaders
  novos compilam e os numeros acima sao dessas medicoes;
- **testes novos**: `effect_quality_test`, `effect_chain_test` (ordem dos passes, a
  nitidez fora do historico), `white_point_test` (espelho do shader), DLAA/DLSS
  nao selecionaveis no `overlay_bindings_test`, `taa=4` valendo 2 no
  `config_load_test`;
- **guardas**: pinos dos shaders novos, ordem da cadeia, historico do resolve,
  DLAA/DLSS nao selecionaveis, shaders de efeito no pacote, ponto de branco;
- menu rasterizado fora do jogo com a lista de anti-aliasing aberta.

**Ainda nao rodou no jogo.**

## Pacote 0.23.2 - 2026-09-14

**O perfil faz todo o trabalho, e o menu segue as telas de referencia.** A 0.23.1
foi reprovada no jogo: trocar o perfil arrastando slider e as chaves ainda sem
efeito esmaecidas. Detalhes em `references/perfil-photorealism-0.23.2.md`.

### O que muda na imagem

- o grade vem so de `[profile.photorealism.0.23.0]`. Sairam do cfg e do codigo as
  camadas medidas (`base.0.1.2`, `module.visual.0.2.0`,
  `module.rain_overcast.0.3.0`), a camada do usuario (`module.user.0.20.0`) e a
  adaptacao de cor por condicao (`module.condition_adaptation.0.19.0`). Piso de
  preto, joelho de altas luzes, matiz e vinheta ficam neutros, como o perfil ja
  fazia na 0.23.0;
- o conjunto de tom segue `lighting_method`: A usa o conjunto 1, B o 2, C o 3 e D
  o 4. O pacote sai em D, o mesmo conjunto 4 aprovado na 0.23.0. O conjunto 5
  continua lido e nenhuma iluminacao o usa;
- `taa` liga e desliga o resolve temporal do plugin: 0 desliga; 1 e 2 ligam; 3 a 6
  sao DLAA/DLSS, sem suporte, e ligam o resolve do plugin no lugar. O pacote traz
  `taa=4`, copiado do cfg de referencia;
- as secoes `depth.0.6.4`, `module.ssao.0.7.0`, `module.ssao_refinement.0.8.0`,
  `module.ssao_interior.0.9.0` e `module.temporal.0.10.0` sairam do cfg. SSAO e
  resolve temporal continuam rodando com os mesmos valores, agora internos; o SSAO
  so se ajusta por `ssao_intensity` do perfil;
- a deteccao de noite continua, interna, e so pesa a exposicao noturna do
  conjunto.

### O que muda no menu

- sem abas: pagina inicial com iluminacao e qualidade em listas suspensas e botoes
  para Anti-aliasing / Motion blur, Renderizacao / Iluminacao, Cores / Tom,
  Objetos (Superficie, Estradas, Vegetacao) e Upscale FSR, cada pagina com
  Voltar;
- escolha entre opcoes e lista suspensa, chave 0/1 e botao ligado/desligado,
  slider so para faixa continua, sliders inteiros andam de 1 em 1; nada esmaecido;
- Cores / Tom edita o conjunto da iluminacao escolhida, e o Salvar grava
  `tonemap_<controle>_<n>` desse conjunto;
- Restaurar padroes devolve o perfil inteiro aos valores do cfg de referencia;
- sairam as abas Render, Clima e Cena. Observador de cena e bloom continuam no
  cfg; FSR ganhou pagina propria;
- teclado: setas movem e ajustam, Enter abre, liga ou reinicia, Tab volta.

### Guardas que mudaram

Sairam as que fixavam as camadas medidas no cfg (secoes, `black_lift`, `tint`,
`blacks`, perfil cumulativo somado por awk), a da ordem da camada do usuario, a da
trava de cor da adaptacao e as de valor de SSAO/temporal/depth no cfg. Entraram:
secoes aposentadas nao podem voltar ao cfg, valores internos de SSAO e temporal
fixados em `defaults.cpp`, a cor nao pode voltar a vir da adaptacao, e a cor do
menu tem que ir e voltar do conjunto da iluminacao ativa.

### Verificacao

Testes reescritos: `config_load_test` (cfg empacotado igual ao default interno em
todas as chaves do perfil, secoes aposentadas ignoradas, limites),
`photorealism_profile_test` (iluminacao escolhe o conjunto, perfil como unica
fonte, edicao presa ao proprio conjunto, restaurar padroes),
`menu_roundtrip_test` (Salvar do menu de verdade e releitura),
`menu_save_test` e `overlay_bindings_test` (toda pagina alcancavel e com Voltar,
nenhum slider escolhendo entre opcoes, listas andando uma opcao por vez). O menu
foi rasterizado fora do jogo, pagina por pagina e com as listas abertas, e
conferido nas imagens.

**Ainda nao rodou no jogo.**

## Pacote 0.23.1 - 2026-09-14

**Os cinco conjuntos de tom escolhiveis e as demais chaves do cfg de
referencia.** Detalhes em `references/perfil-photorealism-0.23.1.md`.

### O que muda

- os cinco conjuntos de tom do cfg de referencia estao no cfg, com os valores
  copiados como estao; o pacote continua saindo no conjunto 4;
- aba **Perfil** do Ctrl+P com o slider "Conjunto de tom" (1 a 5): trocar o
  conjunto recompoe o grade na hora, a partir do cfg em disco, e o Salvar grava
  `tonemap_set` na secao do perfil. A referencia do menu segue o conjunto, para o
  delta da camada do usuario bater com a tela;
- exposicao noturna aplicada: `exposure + night_exposure * peso_de_noite`, com o
  peso de noite da adaptacao por condicao. A adaptacao passou a medir a cena
  tambem com a cor travada pelo perfil, sem mexer em temperatura e matiz. So o
  conjunto 1 usa (+2 EV);
- as outras 26 chaves do cfg de referencia entram na secao do perfil com os
  valores dele (`lighting_interior=0.17`, `use_sss=1`, `fxaa=1`, `taa=4`,
  `hide_show_key=520`, ...). Sao lidas, mostradas em cinza no menu com `*` e
  listadas no log com o motivo de ainda nao terem efeito; o menu nunca as grava.
  Pre-exposicao, pre-contraste e contraste dinamico do conjunto tambem aparecem
  em cinza, pendentes do HDR do jogo;
- Enter numa linha cinza do menu nao reinicia mais o valor;
- o cfg empacotado nao tem mais comentarios. Nenhum valor mudou; a
  justificativa de cada numero segue neste CHANGELOG, em `references/` e no cfg
  da 0.23.0 no historico do git.

### O que e suposicao

Que a exposicao noturna do cfg de referencia seja um acrescimo em EV que cresce
com a noite. As chaves sem efeito foram copiadas sem interpretar.

### Verificacao

`photorealism_profile_test` ganhou os cinco conjuntos contra a referencia, o cfg
empacotado contra o default interno (conjuntos, `tonemap_set` e as 26 chaves),
arredondamento e limite do conjunto, grade e exposicao noturna seguindo o
conjunto escolhido, chaves pendentes sem tocar no grade e a recomposicao so
quando a escolha muda. `menu_roundtrip_test` grava o conjunto e rele;
`overlay_bindings_test` exige uma linha cinza por chave pendente e o slider do
conjunto gravando na secao do perfil. Guardas novas, quebradas de proposito numa
copia: peso de noite antes da trava de cor, exposicao noturna no shader,
referencia do menu com o conjunto, cinco conjuntos no cfg, Enter em linha cinza,
linhas novas do log. A guarda que lia a ressalva do bloom no cfg passou a le-la
no log do modulo.

**Ainda nao rodou no jogo.**

## Pacote 0.23.0 - 2026-09-14

**Perfil de tom do photorealism-plugin.** O grade pode sair de um perfil com os
valores de um cfg de referencia fornecido pelo usuario, em vez das camadas
medidas. Primeira entrega do plano de efeitos desse cfg; detalhes em
`references/perfil-photorealism-0.23.0.md`.

### O que muda

- secao nova `[profile.photorealism.0.23.0]`, **ligada por padrao**: cinco
  conjuntos de tom (`tonemap_<controle>_<n>`), o escolhido em `tonemap_set`, e
  `sharpness`, `sharpen_edges`, `ssao_intensity`;
- o pacote sai com o conjunto 4: exposicao -0.06, contraste 0.99, saturacao
  1.00, sombras -0.01, altas luzes -0.07, brancos -0.01, temperatura 6500 K;
  nitidez 0.6, contraste local 0.4, SSAO x1.5;
- com o perfil ligado as camadas medidas ficam guardadas no cfg, fora da
  composicao; a camada do usuario do menu continua somada por cima. Desligado,
  o grade volta a ser exatamente o medido;
- a cor fica travada no conjunto: a adaptacao por condicao continua medindo a
  cena, mas nao mexe em temperatura e matiz;
- o multiplicador de SSAO entra so na hora de desenhar. Gravado no ajuste,
  cada Salvar do menu multiplicaria de novo;
- aba **Perfil** no Ctrl+P com o botao de ligar e desligar, que recompoe o
  grade na hora a partir do cfg em disco, sem recompilar shader; a referencia
  do menu segue o botao, para o delta gravado bater com a tela;
- o log mostra o perfil ativo, o conjunto e os valores, e avisa quando o
  conjunto escolhido usa pre-exposicao, pre-contraste, contraste dinamico ou
  exposicao noturna, que ainda nao sao aplicados.

### O que e suposicao

As formulas do cfg de referencia nao sao conhecidas; cada valor entra no
controle homonimo do grade. `sharpness` e `sharpen_edges` vem numa escala
0-10 e entram divididos por 10. Mesmos valores nao garantem a mesma imagem.

### Verificacao

Teste novo `photorealism_profile_test` (chaves por conjunto, chaves
malformadas recusadas, conversao das escalas, composicao com o perfil ligado e
desligado) e um caso novo no `menu_roundtrip_test` com o perfil ligado. Os
testes das camadas medidas passam a desligar o perfil explicitamente. Guardas
novas, quebradas de proposito numa copia: multiplicador de SSAO no desenho e
fora do cfg, cor travada, referencia do menu, camada do usuario por ultimo e
nenhum nome de plugin de terceiros no codigo.

**Ainda nao rodou no jogo.**

## Pacote 0.22.8 - 2026-09-13

**Dois defeitos apontados pela revisao automatica do PR #5.** Os dois foram
reproduzidos antes de corrigidos.

### A escala do jogo voltava errada quando os eixos eram diferentes

O ETS2 guarda a escala em `r_scale_x` e `r_scale_y`, e os presets do menu
grafico usam valores diferentes em cada eixo: o de 75% grava `x=0.75` e `y=1`
(visto no `config.cfg` do usuario na 0.22.4). Ao ligar o FSR, o plugin guardava
so o `r_scale_x`; ao desligar, devolvia esse valor aos dois eixos. Reproduzido
com a logica antiga: `x=0.75 y=1` voltava como `x=0.75 y=0.75` -- 56% no lugar
de 75%, a configuracao grafica do jogador alterada pelo plugin.

Agora os dois eixos sao lidos, guardados e devolvidos separados
(`scale_axes.hpp`, testado no host). O arquivo `config.photorealism-fsr-scale.saved`
passa a ter duas linhas; o de uma linha, das versoes anteriores, continua valendo
para os dois eixos. Um eixo que o jogo nunca escreveu nunca e inventado.

### A copia do quadro interno sobrevivia a troca de dispositivo D3D11

A copia era reaproveitada por tamanho e formato. Se o jogo recriasse o
dispositivo D3D11 com o FSR ligado, o resto do plugin se reiniciava, mas a
captura do FSR nao: o proximo quadro copiava uma textura do dispositivo novo
para dentro de um recurso do antigo e ligava esse recurso no contexto novo --
proibido no D3D11, indefinido no DXVK. Reproduzido na GPU, no DXVK do usuario,
com dois dispositivos reais: com a logica antiga a copia continuava no
dispositivo antigo e `copy_from` dizia que tinha copiado.

Agora a troca de dispositivo libera o FSR e reinicia a captura de cor, como o
depth ja fazia, e a copia so e reaproveitada no mesmo contexto. Nenhuma sessao
do usuario registrou troca de dispositivo; o defeito nao estava aparecendo.

## Pacote 0.22.7 - 2026-09-13

**A granulacao LFGA passa a vir em 0.30.** Escolha do usuario no jogo com a
0.22.6.

### O que o teste da 0.22.6 mostrou

- 93.235 quadros reconstruidos de 1664x936 para 1920x1080, na escala 0.8660;
- o ruido azul da granulacao foi gerado em 43 ms, e nenhum erro de shader ou de
  recurso apareceu;
- o usuario avaliou a imagem como otima e escolheu a granulacao em 0.30.

### A mudanca

`grain` do `[module.fsr.0.21.0]` e o default do codigo foram de 0.15 para 0.30.
A guarda que exige cfg e codigo iguais passou a pedir 0.30.

Quem ja tem o cfg instalado mantem o valor gravado nele.

## Pacote 0.22.6 - 2026-09-13

**O RCAS agora e o da AMD, bit a bit, e o FSR ganhou a granulacao LFGA.** Escala
padrao em 0.8660 (75% dos pixels).

### Uma correcao, antes de tudo

A 0.22.4 disse que o RCAS ja batia com `ffx_fsr1.h`. **Nao batia**, em quatro
pontos: aplicava sempre a reducao de ruido que a AMD deixa desligada
(`FSR_RCAS_DENOISE`), dividia sem a aproximacao media que a AMD usa, tinha
pisos que a AMD nao tem, e lia a nitidez como multiplicador linear em vez de
stops. A 0.22.4 tambem disse que `rcp()` nao existe no compilador HLSL do
Proton: **existe**, e a guarda que proibia `rcp` saiu.

### RCAS

Transcrito de `FsrRcasF`, sem a reducao de ruido, como e o padrao da AMD -- e
ela mesma recomenda granulacao depois do RCAS no lugar da reducao. A nitidez
segue `FsrRcasCon` com o remapeamento da API do FSR 2: 1 e o maximo, cada 0.5 a
menos corta pela metade. O 0.60 escolhido pelo usuario vira multiplicador
0,5743, perto do 0,60 que ele aprovou; sem a reducao de ruido, texturas com
granulo fino ficam um pouco mais afiadas que antes.

### LFGA, a granulacao do FSR

`FsrLfgaF` como esta, depois do RCAS, em espaco linear, com ruido azul 64x64
gerado por void-and-cluster e animado pela razao aurea, para a soma no tempo
nao ter vies. Granulacao monocromatica, como no exemplo da AMD. Novo ajuste
`grain` no `[module.fsr.0.21.0]` e slider "Granulacao LFGA" na aba FSR do
menu, valendo na hora; padrao 0.15. A granulacao so existe enquanto o FSR
reconstroi, e fica abaixo da interface.

### Como foi medido

Na GPU, contra o `ffx_fsr1.h` original compilado pelo compilador do Proton e
rodado no DXVK da RX 6600: RCAS em quatro nitidezes e LFGA, em tres entradas
-- incluindo areas pretas e brancas puras, onde os limitadores dividem por zero
-- com **zero bits diferentes**. O pixel shader inteiro, renderizado de verdade:
sem granulacao, a no maximo 1 byte do RCAS em RTV sRGB e UNORM; com 0.15, 1,16
byte de desvio por quadro e vies de -0,003 byte em 16 quadros. Detalhes em
`references/fsr-rcas-lfga-0.22.6.md`.

### Escala padrao

O cfg do repositorio foi editado pelo usuario para `render_scale=0.8660` e ele
escolheu esse valor como padrao do pacote; o default do codigo acompanha.
Guarda nova exige que cfg e codigo concordem em escala e granulacao.

**Ainda nao rodou no jogo.**

## Pacote 0.22.5 - 2026-09-13

**A nitidez do RCAS passa a vir em 0.60.** Escolha do usuario no jogo, com o
EASU ja igual ao da AMD.

### O que o teste da 0.22.4 mostrou

- 68.971 quadros reconstruidos em 1288x728 -> 1920x1080, sem descarte de
  00:14:55 a 00:33:58. Os 11.223 sem reconstrucao ficam no carregamento, em
  2 min a 60 quadros por segundo sem cena interna logo depois da entrega da
  carga, e em 2 min a 1 quadro por segundo, jogo em segundo plano;
- o usuario avaliou a imagem como otima e comparou a nitidez do RCAS em 0.35 e
  0.60 no jogo, preferindo 0.60;
- nas capturas em 0.60, bordas diagonais continuas, sem halo claro em volta da
  cabine nem das arvores contra o ceu;
- a notificacao de carga pronta aparece por cima da imagem reconstruida: a
  interface desenhada depois do FSR, como a 0.22.3 pos, continua visivel.

### A mudanca

`sharpness` do `[module.fsr.0.21.0]` e o default do codigo foram de 0.35 para
0.60. O "R" do menu passa a devolver 0.60. Guarda nova exige que os dois digam
o mesmo valor.

Quem ja tem o cfg instalado mantem o valor gravado nele: o pacote so muda o
padrao de quem instala ou apaga o cfg.

## Pacote 0.22.4 - 2026-09-13

**O EASU agora e o da AMD, bit a bit.** O shader anterior somava a borda com
`max()` em vez de soma e detectava borda so pelo verde; a adaptacao a borda
chegava a 25% da AMD, e as diagonais saiam em degrau.

### O que o teste da 0.22.3 mostrou

A reconstrucao rodou em todo quadro, antes da interface, sem piscar. O
registro de passes confirmou a posicao: o jogo termina o quadro em 1288x728
(`f29`), faz o proprio upscale no backbuffer, o FSR escreve no bind seguinte e a
interface vem por cima. Em 100% o quadro fecha com 3 passagens pelo backbuffer;
em 44% com 4 -- a a mais e o upscale do jogo.

Continuou borrado. Com a nitidez do RCAS em 0.70 e 1.00 as capturas mostraram
degraus nas bordas diagonais (silhueta do predio, rota no GPS da cabine) e
folhagem com aspecto de pintura -- degrau em diagonal e justamente o que o EASU
existe para suavizar.

### A causa

Comparado com `ffx_fsr1.h` do FidelityFX-FSR2 v2.2.1, o nosso EASU divergia em
oito pontos. O que pesa: a forca da borda usava a **maior** entre horizontal e
vertical, e a AMD **soma** as duas. Depois de `* 0.5` e ao quadrado, a
adaptacao ficava em no maximo 25% da AMD, e a perda e maior na diagonal. A luma
de deteccao era so o verde. Os outros seis -- reciproco aproximado, direcao
nula, `saturate` extra, piso na normalizacao, posicao de amostragem e ordem de
acumulacao -- estao em `references/fsr-easu-0.22.4.md`.

### A correcao, e como foi medida

O shader foi transcrito de `fsrEasuSetFloat`, `fsrEasuTapFloat` e
`ffxFsrEasuFloat`, e as constantes de `ffxFsrPopulateEasuConstants`. As
aproximacoes de reciproco por bits da AMD entram como estao.

A saida foi comparada **na GPU** contra o `ffx_fsr1.h` original, sem alteracao,
compilado pelo mesmo compilador HLSL do Proton do usuario e rodado no DXVK dele
na RX 6600: **zero bits diferentes** em 6,2 milhoes de canais, numa captura do
jogo e num padrao de linhas finas em 26 angulos. A 0.22.3, na mesma comparacao,
diferia em ate 22% da escala na captura do jogo.

Achado no caminho: o LLVM dobrava `0.5 * 1288 * (1/1920) - 0.5` como expressao
fundida e mudava o ultimo bit de `con0[2]`. As constantes agora sao montadas em
instrucoes separadas, e o teste prende o valor IEEE estrito.

O RCAS ja batia com a AMD e nao mudou. `rcp()` nao existe no compilador do
Proton; o shader escreve `1.0 / x`.

### Licenca

O EASU e transcricao de codigo MIT da AMD. A licenca vai em
`references/licenses/` e dentro do pacote, em
`photorealism-plugin/FidelityFX-FSR2-LICENSE.txt`.

### O que continua igual

EASU e espacial: 44% dos pixels continuam 44%. A correcao tira a parte do
borrao e dos degraus que era nossa, nao o limite do FSR 1. Recuperar detalhe e
a Fase 3 do plano, a reconstrucao temporal. **Ainda nao rodou no jogo.**

## Pacote 0.22.3 - 2026-09-12

**A mensagem de dormir piscava com o FSR ligado. A causa e o desenho do RCAS
herdando o estado da interface do jogo, e a correcao move a reconstrucao para
antes da interface.** O registro de passes passa a esperar o mapa carregar.

### O que o teste da 0.22.2 mostrou

Na sessao das 22:23 o FSR reconstruiu pela primeira vez no jogo:
`FSR copia o quadro interno de 1288x728 formato=29` e depois
`fsr.replacement=600` a cada 10 s, sem descarte. Dois defeitos apareceram:

- a imagem piscou na saida da tela de carregamento;
- parado numa area de descanso, a **mensagem de dormir** piscava. Desligar o
  FSR pelo menu parava a piscada na hora.

Na sessao das 21:02 o FSR nao reconstruiu nada em 8 minutos. Ali o depth da
cena ficou em 1920x1080 a sessao inteira: o jogo desenhou em resolucao cheia,
e o borrao daquela sessao era o resolve temporal do plugin, que estava ativo.

### A causa

`draw_rcas` ligava o proprio shader, a view e o viewport, e **nenhum** estado de
blend, depth ou rasterizacao. No `Present` o contexto ainda tem o que o jogo
deixou ligado para o ultimo elemento da interface -- inclusive o scissor com o
retangulo daquele elemento. A reconstrucao cobria so esse retangulo, e so nos
quadros em que ele era o ultimo desenhado. O contador dizia 600 quadros
reconstruidos porque o desenho acontecia; o que ele cobria nao aparecia no log.

A correcao obvia -- ligar estados proprios -- cobriria a tela inteira, e com
ela **a interface inteira**: o jogo desenha o HUD no backbuffer depois do
proprio upscale, e o `Present` vem depois do HUD. Essa era a pergunta em aberto
da 0.22.2, e a resposta e sim.

### A correcao

- o FSR tem estados proprios: blend desligado, depth desligado, rasterizacao
  sem culling e **sem scissor**, ligados em todo desenho do RCAS;
- a reconstrucao saiu do `Present`. Ela roda dentro do `OMSetRenderTargets`,
  na **segunda** vez que o jogo liga o backbuffer depois de passar da
  resolucao interna: a primeira e o upscale do proprio jogo, a seguinte e a
  interface. O FSR escreve por cima do upscale do jogo e devolve o estado; a
  interface e desenhada por cima da reconstrucao, como seria num upscaler
  nativo;
- a saida agora e reconhecida pela **textura do backbuffer**, e nao pelo
  tamanho: um alvo qualquer de 1920x1080 no meio do quadro nunca recebe a
  reconstrucao;
- o sRGB vem da view que o proprio jogo ligou;
- quadro com uma unica passagem pelo backbuffer nao reconstroi, e o log diz
  por que: `o jogo nao voltou ao backbuffer depois de esticar o quadro interno`;
- bind que tambem liga UAVs nao recebe a reconstrucao: restaurar o estado com
  `OMSetRenderTargets` desligaria as UAVs do jogo.

A regra mora em `FrameTransition`, pura. O teste novo modela o quadro medido na
0.22.2 -- backbuffer no inicio, alvos internos, o de tom mapeado, e tres binds
do backbuffer no fim -- e exige a captura no primeiro e a reconstrucao so no
segundo.

### O registro de passes espera o mapa

Na 0.22.2 o registro armava depois de 600 quadros quaisquer e caiu **duas
vezes** na tela de carregamento, com 3 passes por quadro. Agora ele arma depois
de 300 quadros **seguidos** com pelo menos 32 passes -- o jogo dirigindo tinha
72, o carregamento 3 -- e um quadro de carregamento no meio zera a contagem.
Cada linha marca `reconstrucao` no bind em que o FSR escreveu.

### O que continua nao provado

**Nada disso rodou no jogo.** A correcao supoe que o primeiro bind do
backbuffer depois da cena contem so o upscale do jogo. Se o jogo desenhar parte
da interface nesse mesmo bind, essa parte some -- de forma estavel, sem piscar.
O registro de passes mostra os binds; o que cada um desenha, so a tela mostra.

## Pacote 0.22.2 - 2026-09-12

**A imagem piscava entre a cena e um quadro preto com bordas coloridas. A causa
e um pool de texturas do jogo, e a correcao troca "qual textura" por "qual
posicao no quadro".** Mais tres defeitos achados no caminho, e os avisos falsos
do log.

### O que o teste da 0.22.1 mostrou

A cadeia rodou pela primeira vez -- `fsr.dispatch=1050`, o EASU despachando a
cada quadro. E a tela alternava entre duas imagens: a cena normal, e um quadro
quase todo preto com contornos coloridos so nas bordas. Esse segundo e uma
mascara de bordas de anti-aliasing: superficie lisa vira preto, borda vira cor.

O log deu as pistas:

- a cena do ETS2 roda em **1288x728** -- e o depth mais ligado, cerca de 20 mil
  ligacoes por janela de 30 s. O tamanho do candidato estava certo;
- o candidato foi escolhido com `ligado 3 vezes, entre 3 candidatos` -- existe
  mais de uma textura nesse tamanho;
- existe um segundo grupo em **1366x684**, que caia na janela de busca.

### A causa

O Prism3D reaproveita render targets de um pool. A mesma textura **fisica**
recebe papeis diferentes em quadros diferentes: num quadro guarda a cena, no
seguinte guarda a mascara de bordas. A regra da 0.22.0 -- "a textura mais
ligada no tamanho certo" -- prendia uma textura fisica e, com o pool trocando os
papeis, copiava cena e mascara em quadros alternados.

Escolher textura pela identidade e errado por definicao neste jogo.

### A correcao

O que e estavel e a **posicao no quadro**: o ultimo alvo na resolucao interna
antes de o jogo passar para a resolucao de saida. Esse e o quadro que o proprio
jogo vai subir para 1080p, qualquer que seja a textura fisica que o pool deu a
ele naquele quadro.

- a copia acontece **no instante da transicao**, dentro do `OMSetRenderTargets`
  que liga o alvo de saida, **depois** de o bind ser repassado ao jogo -- e nao
  mais no `Present`, quando um alvo intermediario ja pode ter sido reusado;
- o estado fecha a cada `Present`: um quadro sem transicao nao reconstroi nada,
  em vez de reconstruir a copia de um quadro anterior;
- a janela de busca ficou estreita: o tamanho esperado sai da escala com que o
  jogo foi aberto, com tolerancia de 24 px. 1288x728 entra; 1366x684 sai;
- formatos TYPELESS entram na busca. A tabela antiga so listava variante
  tipada -- o mesmo defeito que desligou o observador de cena na 0.18.0.

A regra mora em `FrameTransition`, pura e testada no host. O teste simula o pool
trocando as texturas a cada quadro e exige a cena em todos -- e mostra que a
regra antiga, pela identidade, alternava.

### O que ainda e hipotese, e como a proxima sessao responde

O teste modela a mascara sendo escrita **antes** do resultado final, que e como
mascaras de AA funcionam em geral. Se no ETS2 for o contrario, a nova regra
pegaria a mascara em todo quadro -- estavel, mas errada.

Para nao depender de suposicao, o plugin registra **uma vez por sessao** a
sequencia de passes de dois quadros seguidos, logo que a cena comeca: cada
`OMSetRenderTargets`, tamanho, formato, se vai junto com depth, qual textura
fisica (`#N`), e qual foi capturada. Com esse registro a ordem real do jogo fica
escrita no log.

### Tres defeitos achados no caminho

**O grade era apagado com o FSR ativo.** A ordem era grade -> upscale, e o
upscale substitui o backbuffer inteiro pela reconstrucao do quadro interno, que
nao tem grade nenhum. Agora e upscale -> grade -> captura do Steam -> menu.

**Gamma dupla.** O RTV do backbuffer e sRGB -- o hardware codifica na escrita.
O RCAS le valores ja codificados e os escrevia sem decodificar antes, entao a
imagem reconstruida saia lavada. Agora ele decodifica quando o RTV e sRGB.

**Custo com o FSR desligado.** A observacao de cor da 0.22.0 fazia quatro
chamadas COM em **todo** `OMSetRenderTargets` do jogo, ligado ou nao. Agora ela
sai antes de qualquer chamada quando a captura esta inativa.

### Os avisos falsos do log

- `fsr.replacement=0` era zero **por construcao**: quem o incrementava era o
  hook de backbuffer removido na 0.21.5. Agora conta os quadros de fato
  reconstruidos;
- `aquisicoes_do_jogo=0` idem -- o contador saiu;
- `motivo=quadro interno ainda nao localizado` continuava aparecendo depois de o
  quadro ser achado. Agora o motivo e o da **janela** do relatorio, e vira
  `nenhum` quando a janela nao teve descarte;
- `FSR ligado e sem efeito` repetia a cada 10 s. Agora sai uma vez quando o
  modulo para de reconstruir, e volta a valer quando ele retoma.

A guarda do validate so conferia que o nome `record_replacement` existia -- e a
definicao existia, sem chamador. Ha guarda nova exigindo que **todo**
`Telemetry::record_*` tenha chamador fora do proprio arquivo.

### Onze guardas novas, todas quebradas de proposito

Ordem do Present; captura pela posicao; o teste do pool com a regra antiga;
copia depois do bind; zero COM com a captura inativa; fechamento do quadro no
Present; tabela so de tipados; decode do RCAS; contagem de quadros
reconstruidos; contador sem chamador; motivo limpo por janela.

### O padrao

O pacote volta a sair com `enabled=false`, que foi a escolha do usuario.

### O que continua nao provado

**Nada disso rodou no jogo.** E ha uma pergunta que so o registro de passes
responde: o upscale substitui o backbuffer inteiro no `Present`. Se o ETS2
desenha o HUD **no backbuffer depois** do proprio upscale, a reconstrucao cobre
o HUD. O registro mostra se ha passes no alvo de saida depois da transicao.

## Pacote 0.22.1 - 2026-09-12

**O pacote passa a sair com o FSR ligado, e o comentario do cfg passa a
descrever o mecanismo que existe.**

### A documentacao estava descrevendo um mecanismo removido

O bloco do `[module.fsr.0.21.0]` dizia:

> COMECA DESLIGADO. O caminho troca a textura que o jogo recebe no lugar do
> backbuffer, e isso e invasivo demais para ligar sozinho na primeira versao.

Essa substituicao de backbuffer **saiu na 0.21.5**. A justificativa para comecar
desligado morreu junto e o texto ficou. Quem lesse o cfg estaria lendo sobre
codigo que nao existe mais.

O bloco agora descreve o caminho real: o proprio ETS2 reduz, via `r_scale` no
config dele; o plugin toma essa chave emprestada e devolve ao desligar; e a
reconstrucao procura o alvo de cor interno entre os render targets do jogo.

### Ligado por padrao

Com o mecanismo invasivo fora, nao ha mais razao para o pacote sair desligado.
`enabled=true`.

O que isso significa na pratica: **instalar e abrir o jogo passa a mexer no
`r_scale` do seu config**, guardando o valor anterior em
`config.photorealism-fsr-scale.saved` e devolvendo quando o modulo for
desligado. Quem nao quiser, `enabled=false` antes da primeira abertura -- e ai o
plugin nao toca em nada.

### Uma divergencia que estava para acontecer

O default interno do codigo dizia `false` enquanto o cfg passava a dizer `true`.
Quem apagasse o cfg ganharia comportamento diferente de quem nao apagasse, sem
nenhum aviso. Os dois agora concordam, e ha guarda comparando o valor no cfg
empacotado com o do `defaults.cpp`.

## Pacote 0.22.0 - 2026-09-12

**A peca que faltava para a Fase 1 do plano: achar o quadro interno do jogo.**
E a correcao do spam de log que o teste da 0.21.6 revelou.

### Por que o upscale nunca rodou

Com `r_scale`, o ETS2 desenha a cena reduzida e **ele mesmo** sobe para 1080p
antes de o plugin ver o backbuffer. Quando a imagem chegava ate nos, o detalhe
ja tinha sido perdido. O EASU precisava ler o buffer **antes** desse upscale --
que e exatamente o que o diagrama do plano diz: a entrada e o **Color buffer**,
nao o backbuffer.

### Como o quadro interno e achado

O plugin ja fazia isso para o depth. O `OMSetRenderTargets` esta enganchado
desde a 0.6.0 e agora cataloga render target de cor tambem, na mesma chamada --
**nenhum hook novo**, e nenhum dos oito hooks per-draw removidos na 0.15.0
volta.

A janela de busca e estreita de proposito: textura menor que a saida, com pelo
menos metade de cada lado, sem MSAA e num dos quatro formatos de cor
suportados. Entre os que sobram, vence o mais ligado ao longo do frame -- o
alvo da cena e ligado muito mais vezes que espelho ou mapa de sombra.

Isso e mais determinista que o problema que o plano cita sobre o ReShade:
**sabemos o tamanho que estamos procurando**, porque fomos nos que pedimos ao
jogo para desenhar nessa fracao.

Achado o candidato, ele e copiado a cada apresentacao -- do mesmo jeito que o
`depth_capture` ja copia o depth -- e a copia vira o SRV que o EASU le.

O log diz qual venceu:

```
FSR achou o quadro interno do jogo: 1280x720 formato=87, ligado N vezes,
entre M candidatos na janela de busca.
```

### O spam do slider

Arrastar "Escala de render" no menu rendeu **76 linhas** de log num unico
arraste: `configure()` registrava uma linha por movimento do mouse. Agora so a
troca de ligado/desligado merece linha -- o valor da escala esta na tela e sai
no log do bootstrap quando e aplicado. Ha guarda proibindo o retorno.

### O que medir agora

Se o candidato escolhido for o alvo da cena, `fsr.dispatch` sobe e a cadeia
`720p -> EASU -> RCAS -> 1080p` fecha. Se vier o tamanho errado, o log nomeia o
que foi escolhido e o ajuste e na janela de busca, nao no desenho.

## Pacote 0.21.6 - 2026-09-12

**A 0.21.5 apagava o Scaling das opcoes graficas do usuario a cada abertura do
jogo.** Defeito meu, e o pior tipo: escrita num arquivo do usuario, sem
desfazer.

### O que estava errado

O `r_scale_x`/`r_scale_y` do ETS2 e o **Scaling** das opcoes graficas -- config
do usuario. A 0.21.5 escrevia essas chaves no bootstrap **em toda abertura,
mesmo com o FSR desligado**, forcando `1.0`. Quem tinha Scaling em 83% perdia
isso toda vez que o jogo iniciava.

Do log do usuario:

```
15:10:20  FSR escala: r_scale_x/y de 1 para 1.0000   <- FSR DESLIGADO, e escreveu
```

E como qualquer chave grafica escrita pelo plugin tira o perfil do preset, o
ETS2 grava `g_gfx_quality "-1"` e a tela passa a mostrar **Personalizado** em
vez de "Ultra" -- que e a impressao de "resetou tudo".

### O contrato agora e emprestimo, nao escrita

- **FSR desligado e nada emprestado** -- o plugin **nao toca** no `r_scale`;
- **ao ligar** -- guarda o valor que era do usuario num arquivo ao lado do
  config (`config.photorealism-fsr-scale.saved`) e so entao escreve o seu;
- **ao desligar** -- le esse arquivo, **devolve** o valor original e apaga o
  registro.

O log diz qual das tres aconteceu, com o valor de antes e o de depois.

Cinco guardas prendem o contrato, e duas delas foram quebradas de proposito
para provar que disparam: escrever com o FSR desligado, e pegar emprestado sem
guardar o valor.

### Uma nota sobre o que eu errei

O que faltou nao foi cuidado na escrita -- havia guarda para a escrita ser
atomica desde a 0.12.2, e ela funcionou. Faltou perguntar **se o plugin devia
escrever**, e nao so **como**. Config do jogo e do usuario; o plugin pode pedir
emprestado e tem que devolver.

## Pacote 0.21.5 - 2026-09-12

**O mecanismo estava errado. A caixa "Frame em menor resolucao" do plano e uma
config do proprio ETS2, nao um truque no backbuffer.**

O usuario apontou o diagrama e tem razao. A entrada da reconstrucao, no plano
dele, e **Color buffer / Depth buffer / Motion vectors** -- os buffers internos
do jogo. Eu fui interceptar o **backbuffer**, que e a etapa final. E o "frame em
menor resolucao" e o Prism3D desenhando menor, nao o plugin enganando o jogo
sobre o tamanho da tela.

### O que faltava estava no config do jogo

`r_scale_x` e `r_scale_y` sao a escala interna do ETS2 -- o "Scaling" das opcoes
graficas. O config antigo do usuario tinha `0.833333`. Eu tinha esse arquivo
aberto na tela duas mensagens antes e nao fiz a ligacao.

O plugin ja sabe escrever no config do jogo: o `native_aa` faz isso no
bootstrap, com backup e escrita atomica, antes de o ETS2 abrir. A escala entrou
pelo mesmo caminho.

Com `[module.fsr.0.21.0] enabled=true` e `render_scale=0.6667`, o bootstrap
grava `r_scale_x/y = 0.666700` e o **Prism3D sombreia 921.600 pixels em vez de
2.073.600**. O ganho de quadro vem dai, e nao depende de nada que o plugin faca
por frame.

### O que saiu

A substituicao do backbuffer foi removida inteira -- hooks de `GetBuffer`,
`GetDesc` e `GetDesc1`, a textura interna e o estado que os acompanhava. Alem de
ser o mecanismo errado, ela **empilharia** com o `r_scale`: o jogo desenharia a
480p achando que estava em 720p. Ha guarda proibindo que volte.

O que ficou de `src/hooks/back_buffer_proxy` foi so o acesso ao backbuffer que a
captura do Steam e o menu usam, agora em `present_target`.

### O que ainda nao esta pronto

O EASU nao tem de onde ler. Com o `r_scale`, o ETS2 desenha a cena reduzida e
**ele mesmo** sobe para 1080p antes de o plugin ver o backbuffer -- o detalhe ja
se perdeu ali. Reconstruir com qualidade exige achar o color buffer interno
antes desse upscale, que e a proxima etapa e usa a mesma maquinaria que ja acha
o depth.

Ate la a aba FSR diz exatamente isso:

```
LIGADO  o ETS2 desenha reduzido; reconstrucao propria pendente
```

Ou seja: **o ganho de FPS existe agora**; a reconstrucao melhor que a do jogo
vem depois. Preferi entregar a metade que funciona a continuar prometendo a
inteira.

## Pacote 0.21.4 - 2026-09-12

**A aba FSR passa a mostrar o estado real na tela.** Quatro pacotes e o usuario
nunca conseguiu saber, sem abrir o log, se o upscale estava rodando -- o menu
mostrava o interruptor ligado enquanto nada acontecia.

A primeira linha da aba agora diz uma de tres coisas:

```
ATIVO  1280x720 reconstruido
LIGADO mas inativo -- reinicie o ETS2 para valer
desligado
```

### Por que "reinicie" e nao "funciona agora"

O log da 0.21.3 fechou a questao da ordem, e ela esta certa:

```
14:42:08.848  hooks instalados, inclusive o de GetBuffer
14:42:09.944  primeiro ResizeBuffers do jogo      -- 1,1 s depois
14:42:09.948  ResizeBuffers 1920x1080
```

O hook entra antes de o ETS2 pegar o backbuffer. Com `enabled=true` no cfg na
hora da abertura, a textura interna e entregue e o upscale roda.

O que nao da para fazer e trocar isso no meio da sessao: o ETS2 pega o
backbuffer cinco vezes na abertura e **nenhuma** nas horas seguintes -- medido,
nao suposto, pelo contador `aquisicoes_do_jogo`, que ficou parado em 5 durante
sete minutos de jogo. A resolucao em que um quadro e desenhado se decide quando
o alvo e adquirido, e enquanto o jogo nao adquirir de novo, nada que o plugin
faca muda isso.

Mexer no interruptor continua valendo a pena: ele grava no cfg e vale na
proxima abertura. A diferenca e que agora a tela diz isso, em vez de o usuario
ter que deduzir do log.

## Pacote 0.21.3 - 2026-09-12

**O log da 0.21.2 provou que o upscale nunca rodou, e mostrou tres coisas de
uma vez.** A tela nao mudou; o que mudou foi o rotulo no menu.

### O que o log disse

Duas linhas de FSR em vinte minutos de jogo: `desligado` na abertura e `ligado`
as 14:09:35, quando o usuario ligou no menu. **Nenhuma** linha periodica com
`fsr.replacement` e `fsr.dispatch`. O custo de GPU do passe ficou em 1,0-1,4 ms
antes e depois, sem o degrau que um dispatch de EASU e um grade em 44% dos
pixels teriam deixado.

### 1. O ETS2 pede o backbuffer uma vez, e so na abertura

Os seis `ResizeBuffers` estao todos entre 14:08:15 e 14:08:16. Depois disso, em
vinte minutos, o jogo nao pediu o backbuffer nenhuma vez. A trava da 0.21.2 --
esperar o jogo pegar a textura interna -- estava certa e nunca teve chance de
disparar.

### 2. A config chegava meio segundo tarde

E o defeito de verdade. O jogo pega o backbuffer as **14:08:16.607**, logo apos
o ultimo `ResizeBuffers`. O plugin so lia a configuracao as **14:08:17.164**,
na adocao do device, que acontece no primeiro `Present` -- **depois**.

Entao nem reiniciar o jogo com `enabled=true` no cfg teria funcionado: no unico
instante em que o ETS2 pede o backbuffer, o modulo ainda achava que estava
desligado.

Agora a configuracao e lida na **instalacao dos hooks**, que no mesmo log
acontece as 14:08:15.151 -- um segundo e meio antes do jogo pedir. Ha guarda
exigindo que continue assim.

### 3. O diagnostico estava atras do caminho de sucesso

O contador `aquisicoes_do_jogo` existe exatamente para responder "o jogo pegou a
textura?". Mas ele so era impresso **dentro** do upscale -- que so roda quando o
jogo pegou. Ou seja: a unica pergunta que o log precisava responder era a unica
que ele nao respondia.

O relatorio agora sai sempre que o FSR esta ligado, com o motivo do descarte
quando nao ha o que reconstruir. Ha guarda proibindo que ele volte para dentro
do caminho de sucesso.

### O que esperar agora

Ligue no menu, **Salvar no cfg**, e reinicie o jogo. Na proxima abertura as
linhas devem aparecer a cada dez segundos:

```
FSR interno=1280x720 saida=1920x1080 fsr.replacement=N fsr.dispatch=N
aquisicoes_do_jogo=N descartes=N motivo=...
```

Ligar no meio da sessao continua sem efeito neste jogo, e agora isso esta
medido, nao suposto: o log vai dizer `aquisicoes_do_jogo` parado e
`motivo=o jogo ainda nao pegou a textura interna`.

## Pacote 0.21.2 - 2026-09-12

**O menu liga e desliga o FSR ao vivo.** A trava da 0.21.1, que so deixava valer
num `ResizeBuffers`, saiu -- ela protegia contra um perigo real pelo jeito
errado.

### O perigo, e o jeito certo de evitar

Ligar no meio da sessao nao pode reconstruir uma textura interna vazia por cima
do quadro, porque o jogo continua desenhando na textura que **ja pegou**. Eu
tratei isso adivinhando o momento -- so no `ResizeBuffers` -- em vez de
**esperar o fato**.

Agora o modulo nao adivinha: ele registra quando o jogo pega a textura interna.

- o jogo pede o backbuffer e o FSR esta ligado -> recebe a textura interna, e o
  modulo marca que o jogo passou a segura-la;
- o jogo pede e o FSR esta desligado -> recebe o backbuffer real, e a marca cai;
- **o upscale e o grade em 720p so acontecem enquanto essa marca estiver de
  pe.**

Enquanto o jogo nao pedir de novo, ligar no menu nao muda nada na tela -- nem
para melhor nem para pior. Quando ele pedir, engata sozinho. E desligar continua
seguro: o modulo segue reconstruindo o que o jogo ainda desenha na textura
interna, ate ele voltar a pegar o backbuffer real.

Vale para a escala tambem: mudar a escala so recria a textura no momento em que
o jogo pede, nunca por baixo dele.

Tres guardas novas prendem o invariante, uma por caminho: `Upscaler::present`,
`proxy_for_plugin` e `upscale_frame` tem que seguir quem **segura** a textura,
nunca o que foi **pedido**. Trocar qualquer uma por `enabled` derruba o
validate.

### O que isso muda na pratica

Instalar o pacote devolve o cfg ao padrao `enabled=false`, entao ligar pelo menu
e o caminho natural -- e agora ele funciona. Continua sendo verdade que o efeito
aparece quando o ETS2 pedir o backbuffer de novo; a diferenca e que o modulo
nao exige mais que isso seja um `ResizeBuffers`, e nao faz nada de errado
enquanto espera.

O contador `aquisicoes_do_jogo` continua no log e agora responde a pergunta que
importa: se ele crescer depois de voce ligar no menu, o FSR engatou.

## Pacote 0.21.1 - 2026-09-12

**Ligar o FSR pelo menu gravava no cfg e nao chegava ao modulo.** O primeiro log
de jogo da 0.21.0 trouxe uma linha de FSR so, a do startup, e nenhuma depois de
o usuario ligar na aba FSR.

### O fio que faltava

O menu avisava o host quando um ajuste mudava -- mas **so para os campos do
observador de cena**, que eram os unicos com efeito fora do caminho por quadro
quando o menu foi escrito. O FSR e o segundo caso desses, e entrou sem o aviso.

O resultado no log e exatamente o sintoma: `Menu gravou 1 ajuste(s) no cfg: ok`
as 12:19:30, e nenhuma linha de FSR depois. O cfg instalado tem
`enabled=true`; o modulo nunca soube.

O aviso deixou de ser filtrado por grupo. O menu agora diz "isto mudou" e quem
recebe decide o que refazer -- que era como deveria ter sido desde o comeco. Ha
guarda proibindo o filtro de voltar.

### E por que ligar ainda nao vale na hora

Mesmo com o fio, ligar o FSR no meio da sessao nao pode valer imediatamente, e
isso e do problema, nao do codigo: o jogo desenha na textura que **ja pegou**.
Enquanto ele nao pedir o backbuffer de novo, entregar a nossa nao muda nada --
e trocar o alvo por baixo dele, com a textura interna ainda vazia, poria lixo na
tela.

O momento em que o jogo pede de novo e o `ResizeBuffers`. O log da 0.21.0
mostra os quatro do ETS2, todos as 12:18:03, na abertura -- nenhum durante a
partida.

Entao ligar vale ao **trocar a resolucao no ETS2 ou reiniciar o jogo**. O titulo
da aba diz isso, e agora o log tambem, no instante em que voce mexe:

```
FSR pedido: ligado escala=0.6667. So vale quando o jogo recriar o backbuffer
-- troque a resolucao no ETS2 ou reinicie o jogo.
```

### Um contador a mais, para a proxima duvida

A linha periodica ganhou `aquisicoes_do_jogo`, que conta quantas vezes o jogo
pediu o backbuffer -- separando as chamadas dele das nossas, que acontecem todo
quadro. Se esse numero crescer por quadro, ligar ao vivo passa a ser possivel e
a trava sai. Se ficar parado depois da abertura, esta confirmado que so
`ResizeBuffers` serve, e ai e limitacao do jogo e nao escolha minha.

## Pacote 0.21.0 - 2026-09-12

**FSR de volta, Fase 1 do plano escrito pelo usuario: o jogo desenha em 1280x720
e o plugin reconstroi 1920x1080 com EASU em compute shader.**

### O que o modulo removido na 0.15.0 nao fazia, e este faz

Aquele modulo eram 5.833 linhas cuja propria telemetria dizia
`replacement=0 dispatch=0` -- observava recursos do jogo e nunca substituia
nada. A diferenca desta versao nao e de tamanho, e de mecanismo: em vez de
observar, ela **troca a textura que o jogo recebe**.

O `IDXGISwapChain::GetBuffer` passa a devolver uma textura nossa no tamanho
interno no lugar do backbuffer, e o `GetDesc`/`GetDesc1` passam a relatar esse
tamanho. O Prism3D desenha o quadro inteiro em 1280x720 sem saber -- e essa e a
unica parte que reduz trabalho de verdade. A 0.6667, sao 921.600 pixels
sombreados em vez de 2.073.600.

O grade do plugin tambem passa a rodar em 720p, de graca: 44% dos pixels.

### Os dois passes

**EASU em compute**, grupo 8x8, doze taps com analise de direcao e comprimento
de borda e kernel anisotropico alinhado a ela, clampeado ao 2x2 central. E
reimplementacao em HLSL proprio do algoritmo publicado da AMD -- o repo continua
sem uma linha de terceiros, e o clone do FSR2 serviu de referencia, nao de
fonte.

**RCAS em pixel shader.** Nao por preguica: um compute precisa de UAV, e o
backbuffer do jogo nao e criado com `DXGI_USAGE_UNORDERED_ACCESS`, entao nao ha
UAV para escrever nele. O RCAS le a saida do EASU e escreve direto no RTV do
backbuffer real, o que de quebra economiza uma textura de 1080p e um passe de
copia.

### O que muda na ordem do Present

```
process_frame          grade em 720p, na textura interna
upscale_present_frame  EASU + RCAS -> backbuffer real
observe_postprocessed  captura do Steam, ja reconstruida
draw_overlay_frame     menu, por cima de tudo
```

A captura do Steam e o menu precisam do backbuffer **de verdade**, nao da
textura interna, entao os dois passaram a pedi-lo por um caminho que desvia do
proprio hook. Ha guarda para os dois.

O `SavedState` ganhou o estagio de compute -- shader, SRV, UAV e constant buffer
--, senao o proximo dispatch do jogo herdaria a UAV do upscale.

### Duas guardas que mudaram

A guarda de nao-retorno do FSR saiu, porque o usuario pediu o modulo de volta.
A **licao** dela ficou, virando guarda ao efeito: o modulo e obrigado a contar
`fsr.replacement` e `fsr.dispatch` e por os dois no log. Sem contador, um FSR
que nao roda e indistinguivel de um que funciona -- foi exatamente assim que o
anterior viveu ate ser apagado. Os oito hooks per-draw continuam proibidos; os
do swap chain rodam por resize, nao por draw.

A outra nao era sobre FSR. O awk que confere o perfil de cor somava **qualquer**
chave de **qualquer** secao `[module.*]` no total do grade, e passava so porque
nenhuma chave de modulo tinha nome de campo de cor. O `sharpness` do RCAS foi o
primeiro a colidir: 0.35 entrava na nitidez e o perfil acusava 0.550 onde o
medido e 0.200. Agora as quatro camadas somadas sao nomeadas uma a uma.

### O que nao esta provado

**Nada disso rodou no jogo.** O que esta provado: compila, linka, os tres
shaders passam pelo `d3dcompiler_47.dll` do Wine, o disassembly do EASU mostra
`dcl_uav_typed_texture2d u0` e `dcl_thread_group 8, 8, 1`, os literais
sobrevivem no DLL, e renomear `dispatch_easu` ou `hooked_get_buffer` derruba o
build.

**Comeca desligado**, e a razao e honesta: entregar ao jogo uma textura no lugar
do backbuffer e invasivo, e se o ETS2 tirar o tamanho do viewport de algum lugar
que nao seja nem a textura nem o swap chain, o quadro sai cortado. Ligar esta em
`[module.fsr.0.21.0]` ou no menu, aba FSR.

A troca de ligado/desligado e de escala **so vale na proxima mudanca de
resolucao**. Desligar no meio da sessao destruiria a textura em que o jogo ainda
esta desenhando.

O numero que a Fase 1 existe para produzir -- tempo de GPU nativo contra tempo
com upscale -- depende de rodar. As linhas `fsr.replacement` e `fsr.dispatch`
saem no log a cada dez segundos; se as duas vierem zero, o modulo nao rodou e o
log diz o motivo do descarte.

## Pacote 0.20.5 - 2026-09-11

**Tres defeitos apontados na revisao do PR #4, e um quarto que eu encontrei
procurando por eles.** Os tres procedem.

### O "R" de um slider nunca chegava ao arquivo

O mais serio dos tres. Ao salvar, o menu comparava cada ajuste com o **valor de
referencia** para decidir se havia mudanca. Depois de apertar o "R", o ajuste
passa a ser exatamente igual a referencia -- entao nada era escrito, o delta
antigo continuava no `.cfg`, e o valor voltava no proximo carregamento. O menu
dizia que tinha reiniciado o campo e o arquivo discordava.

A comparacao certa nao e com a referencia, e com **o que esta gravado**. O menu
passou a guardar o estado do disco ao carregar e a escrever tudo que difere
dele, atualizando essa copia depois de cada gravacao bem-sucedida. Um "R" agora
grava zero no delta do usuario, e reiniciar um ajuste de modulo devolve o valor
padrao ao arquivo.

### A fonte do ponteiro sobrevivia ao fechamento do menu

O menu aceita delta de dois ganchos do DirectInput e trava no primeiro que
reportar movimento, para nao contar o mesmo deslocamento duas vezes. So que essa
eleicao nunca era desfeita: `reset()` limpava os acumuladores e deixava a fonte.
Se o jogo reabrisse o dispositivo e passasse a reportar pelo outro gancho entre
uma abertura e outra do menu, todo delta novo era descartado e a seta congelava
-- o mesmo sintoma da 0.20.3, por outro caminho.

A eleicao agora vale por sessao de menu.

### O primeiro clique se perdia se o mouse nao tivesse mexido

So movimento elegia a fonte. Um clique dado sem mexer o mouse antes -- o caso de
quem abre o menu e clica direto no que esta sob o centro da tela -- guardava o
nivel do botao mas nao elegia ninguem, entao o menu continuava lendo o ponteiro
pelo caminho da janela e a borda de clique do DirectInput nunca chegava a
interface.

Agora clique elege a fonte igual a movimento. O nivel do botao tambem so e
aceito da fonte eleita, o que antes deixava um gancho rejeitado sobrescrever o
estado do botao do gancho valido.

### O quarto, que apareceu ao escrever a guarda

Procurando onde registrar o teste novo, descobri que o `menu_roundtrip_test`
tinha parado de rodar: uma edicao da 0.20.4 removeu um bloco do `validate.sh` e
levou o registro dele junto. O arquivo continuava no repo dando impressao de
cobertura.

Agora ha guarda exigindo que **todo `*_test.cpp` em `tests/` apareca no
`validate.sh`** -- a mesma ideia da guarda que exige todo `.cpp` de `src/` em
`build.sh`, que nasceu do mesmo tipo de acidente na 0.19.4.

### Testes

`menu_save_test` e novo, seis casos sobre o caminho de gravacao com o IO
substituido por memoria: o "R" de um slider de cor chega ao arquivo, o de um
ajuste de modulo tambem, salvar sem mudanca nenhuma nao escreve nada, um modulo
desligado e gravado, os comentarios de calibracao sobrevivem, e os dois
controles inertes de bloom nunca sao escritos.

`overlay_pointer_feed_test` ganhou tres casos para os outros dois defeitos.

Os cinco casos foram verificados ao contrario: com o codigo antigo restaurado,
cada um falha. Os primeiros que escrevi passavam por erro de compilacao e nao
por assercao, o que nao prova nada.

## Pacote 0.20.4 - 2026-09-11

**A seta parou de andar porque eu mesmo cortei a fonte de dados dela. Dois
defeitos meus, e um caminho novo que nao depende de nenhum dos dois.**

### O primeiro defeito: matei o que alimenta o DirectInput

A 0.20.2 removia o registro de entrada bruta do processo enquanto o menu
estivesse aberto. No Wine e **de la que o proprio DirectInput se alimenta** --
removendo o registro, o buffer do mouse passou a chegar zerado. A camera parou
de girar por isso, nao pela porteira, e a seta do menu parou junto.

A remocao saiu inteira, e o modulo dela tambem: sem chamador, ele seria codigo
morto. Ha guarda proibindo `RIDEV_REMOVE` em todo o `src/`, com a razao escrita
ao lado.

A porteira de vtable continua, e e ela que silencia o mouse para o jogo -- os
dados chegam, o jogo recebe zero, o menu le o delta.

### O segundo defeito: travei a fonte do ponteiro no relatorio vazio

O menu aceita delta de dois ganchos e trava no primeiro que aparecer, para nao
contar o mesmo movimento duas vezes. So que ele travava tambem num relatorio
**sem dado nenhum**: se o jogo chama `GetDeviceData` sem eventos antes de
`GetDeviceState`, a trava caia na fonte errada e todo movimento real era
descartado depois.

Agora so trava um relatorio que carregue movimento de verdade. Os botoes
continuam passando antes disso, para um clique sem mexer o mouse nao se perder.

### O caminho que nao depende de mouse

Tres versoes seguidas o caminho do mouse quebrou por uma razao diferente, e em
todas o menu ficou inutilizavel. Entao o menu passou a andar por teclado
tambem, o que nao depende de DirectInput, de entrada bruta nem de posicao de
cursor:

- **setas para cima e para baixo** trocam a linha selecionada, rolando a pagina
  sozinha para mante-la visivel;
- **setas para os lados** ajustam o valor em passos de 1% da faixa;
- **enter** ou **espaco** liga e desliga um interruptor, ou devolve um slider ao
  valor de referencia;
- **tab** troca de aba.

A linha selecionada fica destacada e clicar numa linha com o mouse tambem a
seleciona, entao os dois caminhos funcionam juntos. O cabecalho da pagina
mostra os atalhos.

Ha guarda exigindo que esse caminho exista. Escrita primeiro com `grep -F`, ela
nao pegou a propria quebra de teste: renomear `kKeyLeft` para `kKeyLeftRemovido`
mantem a substring. Agora casa palavra inteira.

## Pacote 0.20.3 - 2026-09-11

**A camera parou de girar, mas a seta do menu travava no centro da tela. A
causa e a mesma coisa que consertou a camera.**

O DirectInput em modo relativo funciona porque alguem reposiciona o cursor do
sistema no centro da janela a cada quadro -- e assim que o driver mede
deslocamento sem o cursor bater na borda da tela. O menu lia a posicao com
`GetCursorPos`, entao lia o centro, quadro apos quadro.

### Posicao nao serve; delta serve

O menu deixou de perguntar **onde** o cursor esta e passou a somar **quanto ele
andou**. O ponteiro e nosso, comeca no centro da tela quando o menu abre, anda
pelos deltas e nao sai da tela.

E os deltas ja passavam pelas nossas maos: sao o mesmo buffer que a porteira da
0.20.2 zera para o jogo. Agora ele e lido **antes** de ser zerado, nos dois
ganchos -- `GetDeviceState`, para quem le estado, e `GetDeviceData`, para quem
le buffer. Os dois somam no mesmo ponteiro, mas a primeira fonte que aparecer
trava as outras: contar as duas dobraria a velocidade da seta.

Essa ordem nao aparece em teste nenhum e um dia alguem inverte as duas linhas,
entao ha guarda comparando as posicoes **dentro de cada gancho**. Escrita
comparando o arquivo inteiro primeiro, ela acusou codigo saudavel: os dois
ganchos moram no mesmo arquivo e a comparacao cruzava funcoes diferentes.

### Um chute a menos

Acumular delta so vale se o eixo estiver em modo relativo. Em modo absoluto
`lX` e `lY` sao posicao, e somar daria uma seta fugindo pela tela.

Em vez de supor, a porteira **pergunta**: `GetProperty(DIPROP_AXISMODE)` na
primeira leitura, que e quando o jogo ja configurou o dispositivo. Se vier
absoluto, o menu volta sozinho para a posicao do cursor do sistema, que e o
caminho certo nesse caso. O log diz qual dos dois.

O mesmo vale se nao houver DirectInput nenhum: sem delta nunca chegando, o menu
usa `GetCursorPos` como antes.

### Testes

`overlay_pointer_feed_test`, sete casos: a seta comeca no centro, os deltas
somam em vez de substituir, ela nunca sai da tela, o botao devolve borda e nao
so nivel, a roda volta em entalhes, a primeira fonte trava as outras, e um
alimentador sem dado nenhum se declara inativo.

## Pacote 0.20.2 - 2026-09-11

**O mouse continuava girando a camera com o menu aberto. Faltava fechar os dois
caminhos por onde o ETS2 le o mouse, e nenhum deles era mensagem de janela.**

A 0.20.1 engoliu `WM_INPUT` e mesmo assim a camera girava. Isso ja era resposta:
se engolir a mensagem nao muda nada, o jogo **nao le o mouse por mensagem de
janela**. Sobravam dois caminhos, e esta versao fecha os dois de uma vez em vez
de tentar um e esperar o proximo teste.

### DirectInput, que e provavelmente o caminho de verdade

O plugin entra no jogo como `dinput8.dll` -- ele so funciona porque o ETS2
importa essa DLL. E o ETS2 deixa mapear eixo de mouse para a direcao, que e
coisa de DirectInput, nao de `WM_MOUSEMOVE`.

Entao a propria DLL de bootstrap passou a ser porteira. Em vez de embrulhar as
41 funcoes das duas interfaces COM, usa a mesma tecnica de vtable que os hooks
de DXGI ja usavam: tres slots, nao 41.

- `IDirectInput8::CreateDevice`, slot 3 -- ve todo dispositivo que o jogo cria;
- `GetDeviceState`, slot 9 -- zera o buffer com o menu aberto;
- `GetDeviceData`, slot 10 -- devolve zero eventos com o menu aberto.

O tipo vem do proprio `GetCapabilities`, nao do GUID pedido, entao o volante e
os pedais continuam passando mesmo que o jogo use GUID de instancia. A vtable e
compartilhada, entao os ponteiros de mouse e teclado ficam num conjunto e so
eles sao silenciados.

O `dinput8.dll` pergunta ao `dxgi.dll` se o menu esta aberto, por
`GetProcAddress` em dois exports novos. Um erro de digitacao nesse nome nao
quebraria build nem link -- voltaria nulo e a porteira ficaria muda -- entao ha
guarda cruzando os nomes que o `dinput8` procura com os que o `dxgi.def`
publica.

### Entrada bruta, pelo registro e nao pela mensagem

Engolir `WM_INPUT` so funciona se a nossa subclasse estiver na janela certa.
Mais robusto e tirar o registro: `GetRegisteredRawInputDevices` para ler o que o
jogo registrou, `RIDEV_REMOVE` nas entradas de mouse e teclado enquanto o menu
estiver aberto, e o registro original de volta ao fechar. Nao depende de
adivinhar qual HWND recebe a entrada.

A metade que machuca e a devolucao: sem ela o mouse do jogo morreria depois do
primeiro Ctrl+P. A guarda exige a chamada de registro **dentro** do corpo de
`restore`, porque guardar o nome da funcao nao serve de nada -- um corpo vazio
mantem o nome. Ela foi escrita errada primeiro, do jeito que so olha o nome, e
nao pegou o corpo esvaziado.

### Se ainda assim girar, o log diz por que

Nao ha como eu testar isso aqui, entao cada caminho se declara no log:

- quantos registros de entrada bruta foram suspensos, ou que o jogo nao
  registrou nenhum;
- que tipo de dispositivo o DirectInput criou, um por um, e quais entraram na
  porteira;
- quantas mensagens de entrada bruta a janela engoliu enquanto o menu esteve
  aberto.

Se a camera continuar girando, essas tres linhas dizem qual dos caminhos o ETS2
usa, em vez de eu chutar de novo.

## Pacote 0.20.1 - 2026-09-11

**Menu in-game funcional no Ctrl+P: 64 controles em quatro paginas, ao vivo, com
gravacao no `.cfg`.**

A 0.20.0 abaixo entregou so o esqueleto -- uma janela sem nenhuma opcao. Isso
nao era o pedido e o estagiamento foi decisao errada: reduzir o escopo de uma
entrega e decisao do usuario, nao de quem implementa.

### O menu

Quatro paginas em abas, corpo com rolagem por roda do mouse:

- **Cor e curva de tom**, 17 controles: exposicao, contraste, saturacao,
  vibracao, sombras, altas luzes, pretos, brancos, contraste local, nitidez,
  vinheta, joelho de altas luzes, os tres pisos de preto, e a temperatura e o
  matiz de reserva;
- **Renderizacao e iluminacao**, 27 controles: oclusao de ambiente e seus seis
  parametros, refino em altas luzes, oclusao de interior, resolve temporal e
  bloom, com os quatro interruptores de modulo;
- **Clima e condicao**, 20 controles: o interruptor da adaptacao, a constante
  de tempo, as seis ancoras de sol/chuva/noite e os cinco limiares;
- **Observador e profundidade**, 6 controles.

Cada slider tem rotulo, trilho, valor numerico e um botao **R** que devolve o
campo ao valor de referencia. Os interruptores dizem o proprio estado. Mover
qualquer coisa vale **no quadro seguinte**: os quatro constant buffers ja eram
reenviados a cada `Present`, entao escrever em `settings_` basta. Nenhum shader
e recompilado -- `reload_configuration`, que recompila sete entry points dentro
do `Present`, esta proibida dentro de `src/overlay` por guarda.

Todo valor passa pelo mesmo `apply_limits` do carregador, entao o menu nao
produz estado que o `.cfg` rejeitaria.

### Gravar sem destruir a calibracao

Os 17 campos de cor **nao podem** ser gravados onde estao: o valor efetivo e a
soma de `base.0.1.2` mais dois deltas, resultado de 541 amostras medidas no
ETS2. Um slider gravando ali apagaria a medicao.

Entao eles ganharam uma quarta camada, `[module.user.0.20.0]`, somada por
ultimo. O menu grava **so** nela, e grava a diferenca entre o que esta na tela e
o que as medidas dizem. O "R" zera essa diferenca. As tres camadas medidas
ficam intocaveis -- ha guarda proibindo `src/overlay` de sequer citar o nome
delas.

Os ajustes de modulo, que nao tem camadas, sao gravados no lugar, na propria
secao. O escritor novo e escopado por secao, o que importa porque `radius`,
`intensity`, `bias` e `log_seconds` se repetem em secoes diferentes, e troca
**so o texto do valor** -- o `.cfg` tem ~60% de linhas de justificativa medida
que precisam sair intactas.

Nao havia escritor de INI no repo: o que existia era o do dialeto `uset` do
jogo, que nem insere chave ausente. O IO de arquivo atomico que o AA nativo ja
tinha desceu para `src/config/file_io.cpp`, e o `path_utils` saiu de
`src/native_aa/` -- nao havia nada de AA nativo em juntar dois caminhos.

### A entrada bruta

O menu engolia `WM_MOUSEMOVE` e mesmo assim mover o mouse girava a camera. A
razao: o ETS2 le o mouse por **entrada bruta**, que chega como `WM_INPUT`, e
essa mensagem nao estava na lista. Agora esta, junto com
`WM_INPUT_DEVICE_CHANGE`.

Para nao ficar no achismo, o menu **conta** quantas mensagens de entrada bruta
engoliu e registra o total ao fechar. Se a camera ainda girar com esse numero em
zero, o jogo le o mouse por DirectInput e o proximo passo e outro -- o log
responde qual, em vez de se adivinhar.

### O que continua sem prova

O `[native_aa.0.12.2]` ficou de fora do menu de proposito: ele e aplicado pelo
`dinput8.dll` no inicio do jogo e mexer nele no meio da sessao nao teria efeito
visivel ate reiniciar.

`bloom_threshold` e `bloom_knee` aparecem **esmaecidos** e nao sao graváveis:
`BloomPyramid::render()` nao tem nenhum call site e `bloom_frame()` nunca os
atribui, entao hoje eles nao chegam a lugar nenhum. Mostrar um slider que nao
faz nada seria pior que mostrar um desligado.

**O menu nao pausa o jogo.** Nao ha como pausar o ETS2 de fora.

### Testes

- `menu_roundtrip_test` -- a conta inteira: o valor que o slider mostrava volta
  identico depois de gravar e reler; as tres camadas medidas e os comentarios do
  arquivo saem intactos; uma camada do usuario zerada nao muda nada;
- `overlay_bindings_test` -- **todo** controle das quatro paginas resolve para
  onde sera gravado: os 17 de cor para uma chave `_delta`, o resto para a secao
  e a chave que o proprio leitor usa;
- `config_writer_test` -- nove casos, incluindo chave homonima em outra secao,
  chave comentada, colisao de prefixo (`r_aa` contra `r_aa_quality`) e `\r\n`.

Guardas novas, todas quebradas de proposito para provar que disparam: a camada
do usuario tem que ser a ultima soma; o menu precisa ter pelo menos cinquenta
controles declarados -- uma janela vazia compila e passa em tudo, que foi
exatamente o que aconteceu na 0.20.1.

## Pacote 0.20.0 - 2026-09-11

**Base do menu in-game: abre no Ctrl+P, desenha e responde ao mouse.**
Primeiro de quatro estagios. Este entrega o caminho de desenho, a entrada e a
fonte -- os widgets, as paginas e a gravacao no `.cfg` vem a seguir.

### O que nao existia

Nada disso estava no repo: mouse, WndProc, HWND do jogo, fonte, vertex buffer,
input layout, blend com alpha, textura criada a partir de dados da CPU. O unico
`GetAsyncKeyState` era o de Home/End/Insert e o unico `HWND` era a janela-sonda
escondida que existe so para ler vtables.

### Onde o menu desenha, e por que ali

Terceira chamada no `Present`, **depois** de `observe_postprocessed_frame`:

```
process_frame(swap_chain);
observe_postprocessed_frame(swap_chain);
draw_overlay_frame(swap_chain);
```

Depois da captura, porque a captura do Steam le o backbuffer ja graduado -- se
o menu subisse para antes, ele passaria a ser gravado dentro dos screenshots do
jogo. Fora do `render_frame`, porque o menu nao e parte da imagem graduada e
porque assim ele continua funcionando com o efeito desligado no Home.

### Quatro achados que mudaram o desenho

**`is_processing_frame()` nao cobre quem desenha fora do `process_frame`.** Ela
le um `thread_local` que so o `process_frame` levanta, e os hooks de contexto a
consultam para separar um `OMSetRenderTargets` do jogo de um nosso. Um menu
desenhando sem essa flag entrega o proprio render target ao observador de depth
como candidato. O `draw_overlay_frame` usa o mesmo `ProcessorScope`, extraido
agora em RAII para os dois caminhos.

**O `SavedState` tinha exatamente um buraco no caminho do menu.** Ele ja salvava
RTVs, blend, depth-stencil, rasterizer, viewports, scissors, input layout,
topologia, os cinco shaders, PS SRV 0..3, PS sampler 0..1 e PS constant buffer
0. Nao salvava SRV de vertex shader, constant buffer de vertex shader, vertex
buffer nem index buffer. Escolhendo uma geometria sem IA e sem constant buffer
de vertex, sobrou um unico slot a acrescentar: a SRV do vertex shader.

**Geometria sem tocar o Input Assembler.** Um `StructuredBuffer` dinamico lido
pelo vertex shader por `SV_VertexID`, com as posicoes ja em NDC vindas da CPU --
o mesmo idioma sem-IA do triangulo de tela cheia que o plugin ja usava. Seis
vertices por quad, cantos arredondados por SDF no pixel shader, recorte feito na
CPU. Resultado: um unico `Draw` por frame, nenhum input layout, nenhum scissor
state, e o rasterizer padrao serve como esta.

**O RTV do backbuffer ja resolve sRGB, e o menu herda isso.** O
`create_output_view` tenta um RTV `_SRGB` e cai para codificacao manual quando
ele falha. As cores do tema sao autorais em sRGB, entao o pixel shader decide
pelo mesmo flag, que viaja no PS constant buffer slot 0 -- que ja era salvo.

### Fonte: a embutida primeiro, de proposito

O menu assa um atlas 96x48 a partir de uma tabela 5x7 `constexpr` de 95 glifos.
Nao depende de nada. O atlas por GDI entra no estagio 2, com queda para esta.

A ordem e deliberada: "meu renderer de quads funciona" e "o stack de fontes do
Wine funciona" sao dois riscos independentes, e testar os dois de uma vez nao
diz qual falhou.

### O acordo de layout que nada verificava

O vertex atravessa a fronteira CPU/GPU sem input layout: o shader le o
`StructuredBuffer` por indice. Nenhum compilador liga os dois lados -- um campo
a mais de um lado apareceria so como menu embaralhado na tela. O disassembly
confirma `dcl_resource_structured t0, 64`, batendo com o `sizeof(Vertex)` em
C++, e agora um `static_assert` prende o lado C++ enquanto uma contagem de
floats prende o lado HLSL.

### Guardas novas

- **todo `.cpp` sob `src/` precisa estar em `tools/build.sh`**. E a classe de
  erro da 0.19.4, em que modulos extraidos foram compilados fora do binario e
  build e validate ficaram verdes sobre codigo morto;
- `draw_overlay_frame` nao pode subir para antes de `observe_postprocessed_frame`;
- `draw_overlay_frame` nao pode largar o `ProcessorScope`;
- `SavedState` nao pode perder a SRV do vertex shader;
- `src/overlay` nao pode citar `base.0.1.2`, `module.visual.0.2.0` nem
  `module.rain_overcast.0.3.0`: o menu nunca grava sobre calibracao medida;
- `src/overlay` nao pode chamar `reload_configuration`, que recompilaria sete
  entry points de shader dentro do `Present` a cada slider;
- `dxgi.dll` precisa de `-lgdi32`;
- `overlay.hlsl` entra no `package.sh` e no `shader_check.sh`.

As sete foram quebradas de proposito, uma por vez, e todas dispararam.

### O que esta provado e o que nao esta

Provado: compila, linka, os literais sobrevivem no DLL, o `VSOverlay` e o
`PSOverlay` compilam no `d3dcompiler_47.dll` do Wine, o stride do buffer bate
nos dois lados, e dez testes cobrem geometria, recorte, atlas e largura de
texto.

**Nao provado ainda:** nada disso rodou no jogo. Faltam duas hipoteses a
verificar no ETS2 -- que o cursor do sistema esta livre (o menu desenha o
proprio ponteiro a partir do `GetCursorPos`, o que evita brigar com
`ShowCursor`/`ClipCursor`), e que engolir as mensagens de janela basta para o
caminhao parar de responder. Se o jogo ler teclado por DirectInput, nao basta.

**O menu nao pausa o jogo.** Nao ha como pausar o ETS2 de fora. Abrir dirigindo
a 90 km/h continua dirigindo a 90 km/h.

## Pacote 0.19.13 - 2026-09-11

**A suavizacao da adaptacao por condicao nunca funcionou acima de 64 fps.**
Defeito da 0.19.0, encontrado no primeiro log de jogo dela.

### O que o log mostrou

Quatro amostras, noventa segundos, e o detector foi de `sol=1.000` para
`noite=1.000` e de volta para `sol=0.846`. Com `tau=180s` isso e impossivel: a
mediana suavizada foi de 52,5 para 2,5 em trinta segundos, quando deveria ter
parado em 44,4.

### A causa

`GetTickCount64` tem resolucao de ~15,6 ms. Acima de 64 fps, boa parte dos
quadros chega ao suavizador com `elapsed_seconds == 0`. O codigo era:

```cpp
float alpha = 1.0f;
if (tau_seconds > 0.0f && elapsed_seconds > 0.0f) {
    ...calcula o alpha pequeno...
}
```

Sem tempo decorrido o `if` nao executava e **alpha ficava no inicializador,
1.0** -- um salto completo para a amostra daquele quadro. Nao era suavizacao
parcial: era nenhuma.

Medido com a cadencia real de tiques do Windows, partindo de 52,5 e alimentando
uma tela escura por 30 s:

| fps | antes | depois | quadros com `elapsed=0` |
| --- | --- | --- | --- |
| 60 | 44,25 | 44,25 | 0 de 1800 |
| 120 | **0,00** | **44,25** | 1677 de 3600 |
| 200 | **0,00** | **44,25** | 4077 de 6000 |

A 60 fps funcionava, e foi por isso que a reproducao das 386 amostras -- que usa
passo de 30 s, nunca zero -- nao pegou nada.

### A correcao

Sem tempo decorrido, nada se move. `tau <= 0` continua significando "sem
suavizacao" e salta, que era a intencao original do `tau_seconds > 0.0f`.

`tests/scene_conditions_test.cpp` ganhou dois grupos: um varre 60, 120, 200 e
500 fps com a cadencia de tiques real e exige o mesmo resultado nos quatro; o
outro exige que mil chamadas com `elapsed = 0` nao movam nada. Verificado que
ambos caem contra o codigo da 0.19.0.

### Um erro de refatoracao no mesmo log

A linha de conclusao da descoberta de depth saia como
`scan.resource_evictions=0`. Foi a minha substituicao em massa da 0.19.10, que
renomeou `resource_evictions` para `scan.resource_evictions` **dentro da string
de formato** e nao so no codigo. Corrigido.

### Esta versao muda a imagem

Ao contrario das onze anteriores, esta nao e refatoracao. Com a suavizacao
funcionando, a cor para de perseguir o quadro e passa a levar minutos para
mudar -- que e o que a 0.19.0 pretendia desde o inicio.

## Pacote 0.19.12 - 2026-09-11

`src/native_aa_config.cpp` virou `src/native_aa/`. Sem mudanca de
comportamento. Nenhum arquivo passa de 200 linhas.

```
src/native_aa/
  apply.cpp          174  decidir e aplicar a politica
  config_text.hpp    124  o parser do cfg (ja tinha teste proprio)
  config_file.cpp    115  ler, escrever atomico e fazer backup
  aa_log.cpp          85  o log separado do AA
  game_target.cpp     73  qual jogo esta rodando e onde fica o config.cfg
  policy.cpp          66  ler a politica do photorealism-plugin.cfg
  aa_settings.hpp     37  a tabela dos quatro ajustes
  path_utils.cpp      28  compor caminho com limite de tamanho
```

### Este arquivo era diferente dos outros

Os anteriores tinham `if` encaixados. Este nao: era uma sequencia longa de
retornos antecipados -- 26 `if`, **todos no nivel 1**. O problema era outro:
**142 linhas numa funcao** e a mesma coisa escrita quatro vezes.

Cada um dos quatro ajustes de AA -- `r_aa`, `r_taa_tuning`,
`r_taa_luma_sharpen`, `r_taa_modulated_drr_strength` -- aparecia em quatro
lugares: ler a politica, ler o detectado, comparar, aplicar. Doze ocorrencias
literais das chaves no arquivo.

Agora ha **uma tabela de quatro linhas** e quatro lacos:

```cpp
constexpr AaSetting kSettings[] = {
    {"r_aa", "6"},
    {"r_taa_tuning", "0"},
    {"r_taa_luma_sharpen", "1.5"},
    {"r_taa_modulated_drr_strength", "0.0"},
};
```

As chaves passaram de doze ocorrencias para quatro -- uma por ajuste, na tabela.
Um quinto ajuste passa a ser uma linha em vez de doze.

### Um estado escondido que a separacao revelou

A primeira versao de `game_target.cpp` guardava o diretorio de Documentos numa
variavel global entre `identify_running_game` e `resolve_documents_config` --
duas funcoes que so funcionavam se chamadas nessa ordem, sem nada dizendo isso.
Foi corrigido antes de seguir: o diretorio vive no proprio `GameTarget`, que ja
era passado entre as duas.

### Duas guardas do `validate.sh` mudaram de forma

`policy.aa.c_str()` e `policy.taa_sharpen.c_str()` fixavam **nomes de campo**
que a tabela eliminou. Foram substituidas por uma unica:
`policy.desired[index].c_str()`.

A nova e mais estrita, e nao menos: as duas antigas cobriam dois dos quatro
ajustes e podiam ser satisfeitas deixando os outros dois com valor fixo. A nova
esta no unico ponto que escreve no `config.cfg` do jogo, e cobre os quatro.

O conjunto de marcadores tambem passou a varrer **a pasta inteira** em vez de um
arquivo so.

### Verificacao

Build e `validate.sh` verdes. Os onze testes passam. Cinco modulos verificados
por quebra deliberada.

As mensagens do arquivo antigo foram procuradas no DLL. Tres pareceram ausentes
num primeiro passe e nao estavam: `\Euro Truck Simulator 2`,
`\American Truck Simulator` e os nomes dos executaveis sao **wide strings**, e
`strings` sem `-e l` nao le UTF-16. Conferidas com a codificacao certa, as cinco
aparecem.

## Pacote 0.19.11 - 2026-09-10

`src/steam_screenshots.cpp` virou `src/steam/`. Sem mudanca de comportamento.
Nenhum arquivo passa de 200 linhas.

```
src/steam/
  capture_pipeline.cpp  195  o que acontece a cada frame apresentado
  integration.cpp       182  ligar, desligar e devolver ao overlay nativo
  capture_slots.cpp     125  os dois slots, staging e buffers
  conversion_worker.cpp 113  a thread que converte BGRA em RGB
  capture_gate.cpp       82  o callback da Steam e a deduplicacao
  steam_api.cpp          78  resolver os exports do steam_api64.dll
  capture_slots.hpp      75  CaptureLock e BlockingCaptureLock em RAII
  steam_api.hpp          47  a ABI do CCallbackBase e os ponteiros
```

### O aninhamento sumiu

| | antes | depois |
| --- | --- | --- |
| maior funcao | 114 linhas | **38** |
| `if`/`for` no nivel 4 | 3 | **0** |
| `if`/`for` no nivel 3 | 7 | **0** |
| `Acquire`/`Release` soltos | 9 | **0** |

`observe_postprocessed_frame` tinha 114 linhas e quatro niveis de encaixe: um
`if` de contexto, dentro dele um laco, dentro dele um `if` de pedido, dentro
dele o `GetBuffer`. Virou seis funcoes com nome -- `write_ready_slots`,
`drain_finished_readbacks`, `submit_requested_capture`, `context_is_needed` --
e o corpo lê a sequencia de cima para baixo.

### Tres travas soltas a mao, e o mesmo risco do observador

O `SRWLOCK` era solto em **quatro caminhos de retorno diferentes** dentro da
mesma funcao, incluindo dois que chamavam `return_to_native_capture` logo
depois. `CaptureLock` e `BlockingCaptureLock` fecham isso pelo destrutor.

O `release()` explicito continua existindo, e continua sendo chamado nos mesmos
dois pontos: **antes** de devolver o controle ao overlay nativo. Nao e
descuido -- `return_to_native_capture` chama `shutdown_steam_screenshots`, que
toma a mesma trava. Manter a trava ali daria deadlock.

`GetDevice`/`GetImmediateContext` e `GetBuffer` tambem viraram RAII
(`FrameDevice`, `BackBuffer`), o que eliminou os quatro `release()` manuais do
caminho de erro.

### A ABI da Steam saiu para um header

`CCallbackBase` e os sete ponteiros de funcao viraram `steam_api.hpp`, e a
resolucao dos exports virou `load_screenshots_api`, que devolve um enum de tres
estados em vez de um `bool` mais seis comparacoes repetidas. O `static_assert`
de 16 bytes foi junto, e foi **verificado que ele ainda morde**: mudar o
`padding` do `CallbackBase` para 7 bytes quebra o build.

### Verificacao

Build e `validate.sh` verdes. Os onze testes passam. Cinco modulos verificados
por quebra deliberada. Todas as mensagens de log com formato do arquivo antigo
foram procuradas no DLL construido; a unica que nao aparece e a do
`static_assert`, que e texto de compilacao e nunca esteve no binario.

Tres guardas do `validate.sh` mudaram de alvo. A que proibe o modulo de
consultar teclado -- `VK_` ou `GetAsyncKeyState` -- passou a varrer **a pasta
inteira** em vez de um arquivo so, e com isso ficou mais estrita do que era: a
captura e delegada ao Steam por API, e nenhum dos seis arquivos novos pode
disputar o F12.

## Pacote 0.19.10 - 2026-09-10

`src/resource_observer.cpp` -- 882 linhas, o maior arquivo do projeto -- virou
`src/resource_observer/`. Sem mudanca de comportamento. Nenhum arquivo passa de
200 linhas.

```
src/resource_observer/
  observer_state.cpp    192  catalogo, cache de views e selecao de slot
  discovery_scan.cpp    174  janela, snapshot, selecao e agrupamento
  discovery_report.cpp  166  as linhas de grupo e de recurso no log
  depth_observation.cpp 164  observar bind e atividade do depth
  candidate_access.cpp   71  entregar e invalidar o candidato
  discovery_control.cpp  70  assinatura do backbuffer e reinicios
  discovery.cpp          57  o orquestrador do ciclo
  format_names.cpp       37  DXGI_FORMAT -> nome legivel
  observer_lock.hpp      39  WriteLock e ReadLock em RAII
  types.hpp              69  as quatro structs e as constantes
```

### `finish_discovery_if_due` tinha 309 linhas

Ela fazia cinco coisas em sequencia, sem separacao: decidir se a janela fechou,
tirar um retrato do catalogo, pontuar e escolher o candidato, agrupar por
resolucao, e escrever oito tipos de linha no log. Agora sao cinco funcoes com
nome, e um `DiscoveryScan` que carrega o resultado de uma para a outra.

O orquestrador ficou com **19 linhas** e mostra a forma do ciclo: amostrar uma
vez em cada 256 chamadas, tomar a trava, rodar a passagem, soltar a trava,
relatar.

### A trava virou RAII, e isso corrigiu um risco real

O `SRWLOCK` era tomado e solto **a mao**, com tres `ReleaseSRWLockExclusive`
espalhados por caminhos de retorno diferentes dentro da mesma funcao de 309
linhas. Qualquer `return` novo escrito ali sairia com a trava presa e travaria o
jogo no proximo `Present`.

`WriteLock` e `ReadLock` fecham isso pelo destrutor. O ponto em que a trava e
solta continua exatamente onde estava -- **antes** do relatorio, porque escrever
oito linhas de log segurando a trava do observador atrasaria o `Present` --
agora expresso por um bloco em vez de uma chamada solta no meio.

### Verificacao

O observador nao tem teste unitario: ele so existe enquanto o jogo desenha.
A verificacao foi a mesma dos hooks -- **todas as mensagens de log com formato
do arquivo antigo foram extraidas do commit anterior e procuradas no DLL
construido.** Nenhuma faltou.

Build e `validate.sh` verdes. Os onze testes passam. Cinco modulos verificados
por quebra deliberada.

A guarda que exige o motivo de rejeicao nas linhas de recurso -- `elegibilidade=`
com `bindings-insuficientes` -- foi reapontada para `discovery_report.cpp`,
que e onde essas linhas passaram a ser escritas.

## Pacote 0.19.9 - 2026-09-10

`src/hook.cpp` virou `src/hooks/`. Sem mudanca de comportamento. Nenhum arquivo
passa de 200 linhas.

```
src/hooks/
  hook_install.cpp     169  instalar: sondar, remendar vtables, registrar
  device_probe.cpp     142  a janela oculta e o device de prova, em RAII
  swap_chain_hooks.cpp  84  Present, Present1 e ResizeBuffers
  context_hooks.cpp     69  OMSetRenderTargets*, ClearDepthStencilView
  hook_audit.cpp        65  quem e o dono de cada entrada da vtable
  module_names.cpp      44  endereco -> nome do modulo que o contem
  signatures.hpp        34  os sete tipos de ponteiro de funcao
  hook_state.hpp        36  os atomicos compartilhados e o escopo de despacho
  vtable_patch.hpp/cpp  52  trocar um slot com VirtualProtect
  (mais quatro headers de interface)
```

### O que a separacao encontrou

**A sonda de device virou RAII.** `install_swap_chain_hooks` criava uma janela
oculta, registrava a classe, criava device, swap chain e contexto -- e liberava
os cinco na mao, no fim, depois de tres caminhos de retorno diferentes. Agora e
um `DeviceProbe` cujo destrutor solta tudo. Os `DestroyWindow` e
`UnregisterClassW` espalhados sumiram.

**`replace_vtable_entry` era um template inteiro no `.cpp`.** A parte que mexe
em memoria -- `VirtualProtect`, `InterlockedExchangePointer`,
`FlushInstructionCache` -- nao depende do tipo. Virou `patch_vtable_slot`, uma
funcao normal em `vtable_patch.cpp`, e o template ficou reduzido a converter o
ponteiro e guardar no atomico certo.

**A instalacao virou quatro passos com nome.** `patch_present`,
`patch_present1`, `patch_context` e um `InstallReport` que diz o que entrou. A
funcao de 198 linhas com seis booleanos soltos virou 25 linhas que se leem em
voz alta.

**Os atomicos ganharam namespace proprio.** Eram onze globais no anonimo de um
arquivo so; agora vivem em `hook_state`, e quem os usa escreve
`using namespace hook_state` de proposito, em vez de alcanca-los por acidente.

### Verificacao

Hooks nao tem teste unitario -- eles so existem dentro do jogo. A verificacao
foi outra: **todas as mensagens de log com formato do `hook.cpp` antigo foram
extraidas do commit anterior e procuradas no DLL construido.** Nenhuma faltou.

Duas ficaram diferentes no fonte porque a quebra de linha entre literais mudou;
no binario, onde a concatenacao ja aconteceu, as duas aparecem inteiras e
identicas.

Build e `validate.sh` verdes. Os onze testes passam. Os cinco modulos
verificados por quebra deliberada -- renomear um simbolo de `hook_state.cpp`
produz vinte e um erros de link, que e a medida de quanto aquele arquivo
sustenta.

Duas guardas do `validate.sh` foram reapontadas para
`swap_chain_hooks.cpp`, incluindo a que exige `process_frame` seguido de
`observe_postprocessed_frame` -- a que garante que a captura da Steam veja o
frame **depois** de todos os passes visuais.

## Pacote 0.19.8 - 2026-09-10

`src/config.cpp` e `src/config.hpp` viraram `src/config/`. Sem mudanca de
comportamento. Nenhum arquivo passa de 200 linhas.

```
src/config/
  defaults.cpp      153  os valores medidos das tres camadas
  section_table.cpp 145  as secoes do cfg e suas chaves
  logging.cpp       132  as linhas de perfil, balanco e modulos
  loader.cpp        107  ler o arquivo, compor e expor a API
  grade_fields.cpp  103  os dezessete parametros de cor
  limits.cpp         99  clamps e pares que precisam ficar em ordem
  settings.hpp       76  a struct que todo mundo consome
  text_utils.hpp     38  trim, clamp, to_number, parse_bool
  calibration.hpp    35  CalibrationLayer e CalibrationStack
  section_table.hpp  30  ModuleField e SectionSpec
  (mais cinco headers de interface, de 9 a 13 linhas)
```

### As responsabilidades que estavam juntas

Um arquivo de 766 linhas fazia nove coisas: definir a struct publica, aparar
texto, mapear chave em campo de cor, mapear secao em modulo, guardar os valores
de referencia, limitar faixas, compor as camadas, registrar no log, e ler o
arquivo. Agora sao nove arquivos, e o nome de cada um diz qual das nove.

**`settings.hpp` separou-se de `config.hpp`.** A struct e consumida por todo o
`postprocess/`; a API de carga so interessa a quem inicializa. Quem precisa de
`Settings` nao arrasta mais `load_settings` junto.

**`defaults.cpp` guarda os numeros medidos** -- piso de preto por canal, limiar
do bloom, ancoras de condicao. E o unico arquivo que muda quando uma medicao
nova chega, e agora da para abri-lo sem passar por parser nenhum.

**`text_utils.hpp` ficou header-only e sem estado.** Sao quatro funcoes puras
sobre `char*` e `float`. Virar `.cpp` custaria uma chamada por chave lida sem
ganho nenhum.

### O teste parou de espiar a implementacao

`config_load_test.cpp` incluia `../src/config.cpp` inteiro para alcancar o que
estava no namespace anonimo. Agora ele inclui so `config/config.hpp` e e
linkado contra os seis `.cpp`. O teste passou a exercitar a **interface**, que e
o que ele deveria ter feito desde o inicio -- a inclusao do `.cpp` era um
sintoma do arquivo unico, nao uma escolha.

### Verificacao

Os 132 campos de `Settings` foram despejados de novo, nos dois cenarios
(defaults internos e parse do cfg entregue), e comparados com o despejo guardado
da 0.19.1: **identico nos 132**.

Build e `validate.sh` verdes. Os onze testes passam. Os cinco modulos
verificados por quebra deliberada.

Tres guardas do `validate.sh` mudaram de caminho, e cada uma foi apontada para
o arquivo onde o codigo guardado realmente esta: o piso de preto para
`defaults.cpp`, `kSections` para `section_table.cpp`, `kGradeFields` para
`grade_fields.cpp`. Apontar as tres para o mesmo arquivo teria passado no build
e deixado duas sem guardar nada.

## Pacote 0.19.7 - 2026-09-10

`src/scene_*` virou `src/scene/`. Sem mudanca de comportamento. Nenhum arquivo
passa de 200 linhas.

```
src/scene/
  features.hpp            128  as quatro features a partir dos pixels
  sampler_resources.cpp   161  piramide, staging e queries
  observer.cpp            100  cadencia, log e a linha "Cena 0.18.0:"
  condition_model.hpp      95  limiares, ancoras, pesos e mistura
  sampler.cpp              76  submeter e drenar a leitura assincrona
  condition_smoother.hpp   52  a media exponencial sobre as features
  sampler.hpp              51
  formats.hpp              41  a tabela DXGI legivel/BGRA
  observer.hpp             41
  sample_sink.hpp          17  a abstracao entre amostrador e observador
```

### As tres misturas que a separacao desfez

**`scene_conditions.hpp` guardava duas coisas sem relacao.** De um lado
funcoes puras que classificam a cena e misturam ancoras; do outro uma classe
com estado que suaviza no tempo. Viraram `condition_model.hpp` e
`condition_smoother.hpp`. Quem so quer decidir a cor de uma amostra nao precisa
mais arrastar a maquina de suavizacao junto.

**`scene_observer` era o amostrador e o observador ao mesmo tempo.** Uma metade
cuida de piramide, staging, queries e `Map` -- e so entende D3D11. A outra
decide de quantos em quantos quadros medir e o que registrar -- e nao precisa
entender D3D11 nenhum. Viraram `SceneSampler` e `SceneObserver`.

**A linha de log "Observador de cena 0.18.0 ativo" provava a mistura.** Ela
misturava o que o amostrador sabe (fonte, formato, mip, pixels) com a cadencia
que so o observador conhece (`intervalo=%u frames log=%.0fs`), e por isso o
amostrador lia `interval_frames_`. Agora o amostrador oferece um `Info` e um
`consume_creation_notice()`, e quem escreve a linha e o observador. O texto do
log nao mudou.

### DIP, com um motivo

`SceneSampleSink` e uma interface de verdade, com um metodo virtual. Ela existe
porque o amostrador nao deve saber o que acontece com os pixels que ele
entrega: ele mapeia a textura, chama `on_sample` e desfaz o mapeamento. O
`SceneObserver` implementa a interface e transforma pixels em features.

O custo e uma chamada indireta a cada ~0,5 s, e o ganho e que o amostrador
pode ser exercitado com um sink de teste sem nada de D3D11 do outro lado.

### Verificacao

Build e `validate.sh` verdes. Os onze testes passam. `SceneSampler` e
`SceneObserver` verificados por quebra deliberada.

Quatro guardas do `validate.sh` mudaram de caminho. Duas delas -- a que exige
`scene_formats::is_readable` fora do `.cpp` e a que exige
`return !resources_failed_` -- foram reapontadas para `sampler_resources.cpp`,
e conferido que o codigo que elas guardam esta mesmo la. Sao as guardas que
existem por causa da 0.18.0, quando o observador saiu desligado e o build
seguiu verde.

## Pacote 0.19.6 - 2026-09-10

Terceira rodada de separacao. Nove modulos novos. Sem mudanca de comportamento.

O orquestrador foi de **1623 para 774 linhas** -- e de 2784 no inicio da serie.
`src/postprocess/` tem hoje 33 arquivos; **30 deles cabem em 200 linhas**.

```
  frame_targets/plan          no orquestrador (774)
  frame_resources.cpp    206  texturas de cena, visual e espacial
  gpu_timer.cpp          217  anel de queries e relatorio
  frame_constants.cpp    192  os quatro constant buffers do frame
  shader_library.cpp     176  guarda e adota os oito shaders
  frame_passes.cpp       161  os seis caminhos de composicao
  pipeline_state.cpp     158  buffers, samplers, raster e blends
  frame_log.cpp          154  as linhas de estado por condicao
  temporal_history.cpp   151  historico de cor e de depth
  bloom_resources.cpp    142  niveis e texturas da piramide
  depth_capture.cpp      129  a copia legivel do depth
  shader_compiler.cpp    119  compilar HLSL em blobs
  bloom_pyramid.cpp      112  desenhar o glow e o preview
  condition_adapter.cpp   92  suavizar features e produzir a cor
  depth_liveness.cpp      81  decidir se o depth ainda esta vivo
  device_state.cpp        76  salvar e restaurar o device
```

### O que a separacao revelou

**`frame_constants` e `frame_log` viraram funcoes livres**, nao classes. Elas
nao tem estado proprio: recebem um `FrameConstantsInput` / `FrameLogInput` e
escrevem. Transformar em objeto so para ter um `this` seria cerimonia.

**`FrameLogState` saiu do orquestrador como struct.** Os oito campos de
throttling de log estavam misturados com estado de render; agora sao um bloco
com nome, passado por ponteiro para quem registra.

**`DepthLiveness` nao chama mais quem a chamava.** Antes ela liberava os
recursos de depth de dentro do proprio corpo; agora devolve `expired` e o
orquestrador decide o que fazer. A ordem foi conferida: o release continua
acontecendo antes do `ensure`, como no original.

**`PipelineState` precisou de acessores de endereco.** A API do D3D11 recebe
`ID3D11Buffer* const*`, e um getter por valor nao tem endereco. Os
`*_address()` existem por isso, e nao por gosto.

### Verificacao

Build e `validate.sh` verdes apos cada extracao. Os onze testes passam. Cada
modulo novo foi verificado por quebra deliberada: renomear a funcao principal
produz erro de link.

Duas guardas do `validate.sh` tiveram de mudar de alvo, e o invariante foi
preservado nas duas. A do observador passou a fixar
`frame_resources_.scene_texture()`, que continua sendo a textura pre-grade. A
do cbuffer virou **duas** guardas encadeadas -- `input.temperature =
condition_.temperature()` no orquestrador e `constants.temperature =
input.temperature` em `frame_constants.cpp` -- porque a cadeia agora passa por
dois arquivos e verificar so uma ponta deixaria a outra livre para divergir.

Um erro foi cometido e corrigido: ao extrair `DepthLiveness` eu escrevi o
periodo de graca como 3 quadros; o original e **2**. Conferido contra o commit
anterior antes de seguir.

### O que fica no orquestrador, e por que

As 774 linhas restantes sao a classe, seus membros e passos pequenos -- nenhuma
funcao passa de 71 linhas. O que sobrou e decisao: adquirir alvos do frame,
montar o `FramePlan`, sequenciar as etapas e desmontar. Extrair isso exigiria
passar quase todos os colaboradores num struct de contexto, o que troca um
arquivo grande por um acoplamento igual com mais indirecao.

## Pacote 0.19.5 - 2026-09-10

Continuacao da separacao de `src/postprocess/`. Quatro modulos novos. Sem
mudanca de comportamento.

```
postprocess.hpp         16   interface publica
com_utils.hpp           14   safe_release
depth_capture.hpp/cpp  162   DepthCapture: a copia legivel do depth buffer
format_utils.hpp        70   familias de DXGI: typeless, sRGB, copia de depth
device_state.hpp/cpp   111   SavedState: salvar e restaurar o device
temporal_history       198   TemporalHistory: historico de cor e de depth
gpu_timer              265   GpuTimer: anel de queries e relatorio
bloom_pyramid          293   BloomPyramid: niveis, glow e preview
shader_constants.hpp   114   os cinco layouts de constant buffer
shader_library         368   ShaderLibrary: compilar e guardar oito shaders
postprocessor.cpp     1623   orquestrador
```

O orquestrador foi de **2091 para 1623 linhas**, e de 2784 no inicio da serie.

### Onde a fronteira ficou desta vez

**`BloomPyramid`** recebe um `BloomFrame` -- shaders, sampler, view da cena,
blends, limiar e joelho. Ele nao sabe que existe um `PostProcessor`, e passou a
ser dono do proprio constant buffer, que era o unico que so ele usava.

**`TemporalHistory`** guarda as duas texturas de historico e o bit de validade.
O `invalidate()` devolve se de fato invalidou, e quem registra o motivo no log
e o orquestrador -- porque o motivo e uma decisao dele, nao do historico.

**`DepthCapture`** guarda a copia legivel do depth. A vivacidade -- decidir se
o depth ainda esta sendo usado pela cena -- ficou no orquestrador de proposito:
ela depende do `resource_observer` e do que se quer registrar, e nao do recurso.

**`format_utils.hpp`** reuniu as familias de DXGI que estavam como metodos
privados de uma classe de 2000 linhas. Sao funcoes puras sobre um enum.

### O que continua junto, e por que

`release_depth_capture_resources` mistura soltar texturas com zerar contadores
de throttling de log. So a primeira metade virou `DepthCapture::release()`; os
contadores ficaram onde os logs estao. Separar um do outro deixou visivel que
eram duas coisas.

### Verificacao

Build e `validate.sh` verdes apos cada extracao. Os onze testes passam.

Cada modulo foi verificado por **quebra deliberada** -- renomear
`BloomPyramid::render`, `ShaderLibrary::compile`, `TemporalHistory::ensure` ou
`DepthCapture::ensure` produz erro de link. Se algum fosse codigo morto, o build
seguiria verde, que foi exatamente a armadilha da 0.19.4.

Uma funcao foi truncada durante a extracao dos formatos e o build acusou na
hora; `depth_copy_formats` foi recuperada do commit anterior e conferida linha a
linha contra o original antes de seguir.

## Pacote 0.19.4 - 2026-09-10

`src/postprocess.cpp` virou `src/postprocess/`, com quatro modulos separados por
responsabilidade. Sem mudanca de comportamento.

```
src/postprocess/
  postprocess.hpp        16   interface publica (era src/postprocess.hpp)
  com_utils.hpp          14   safe_release
  shader_constants.hpp  114   os cinco layouts de constant buffer
  device_state.hpp/cpp  111   SavedState: salvar e restaurar o estado do device
  gpu_timer.hpp/cpp     265   GpuTimer: anel de queries, media/pico, relatorio
  shader_library.hpp/cpp 368  ShaderLibrary: compilar e guardar os oito shaders
  postprocessor.cpp    2091   orquestrador
```

O orquestrador foi de 2784 para 2091 linhas, e o que saiu nao foi "um pedaco do
arquivo" e sim tres coisas que nunca precisaram saber do resto.

### Onde a fronteira ficou

**`GpuTimer`** era treze membros e sete metodos entrelacados com o resto da
classe. Ele so precisa de um device e um contexto, e produz uma linha de log a
cada dez segundos. Agora recebe os dois em `attach()` e o orquestrador so
conhece `begin/end/poll/release`.

**`ShaderLibrary`** guarda os oito shaders e sabe compila-los. O orquestrador
pedia `pixel_shader_` direto do proprio corpo; agora pergunta `shaders_.visual()`
e nao tem como escrever nesse ponteiro.

**`device_state`** nao virou classe porque nao tem estado proprio: sao duas
funcoes livres sobre uma struct. Transformar isso em objeto seria cerimonia.

### Sobre DIP, com honestidade

A inversao que vale aqui e a de **dados**: cada modulo recebe o que precisa pela
sua interface, em vez de alcancar os membros de um objeto-deus. `BloomPyramid`
nao sabe que existe um `PostProcessor`; o `PostProcessor` e que depende da
interface pequena de cada modulo.

O que **nao** foi feito, de proposito: interfaces abstratas com metodos virtuais
sobre os tipos do D3D11. Nao ha um segundo backend para trocar, nao ha teste que
use um duble, e o custo seria uma chamada indireta por passe. Seria cerimonia
com nome de principio.

### Verificacao

Build e `validate.sh` verdes apos cada extracao. Os onze testes passam.

Uma armadilha foi encontrada e corrigida no caminho: as primeiras extracoes
copiaram o codigo para os modulos novos **sem remover o original**, que ficou
no namespace anonimo do `.cpp`. Compilava e passava, mas os modulos novos eram
codigo morto -- a copia anonima e que rodava. Foram 202 linhas duplicadas.

Para nao depender de leitura, cada modulo foi verificado por quebra deliberada:
renomear `capture_state` em `device_state.cpp` produz erro de link, e renomear
`GpuTimer::poll` produz dois. Se fossem codigo morto, o build seguiria verde.

## Pacote 0.19.3 - 2026-09-10

Refatoracao de `src/postprocess.cpp`, pedida pelo usuario. Sem mudanca de
comportamento.

| | antes | depois |
| --- | --- | --- |
| **maior funcao** | **694 linhas** | **73** |
| metodos | 34 | 76 |
| `else if` | 13 | 1 |
| `if (` | 174 | 157 |
| `if`/`for` no nivel 4 | 5 | **0** |
| `if`/`for` no nivel 3 | 50 | **25** |

O arquivo foi de 2547 para 2784 linhas. Achatar custa linhas: cada bloco
extraido ganha assinatura e chaves. O que caiu foi a profundidade, que e o que
torna um trecho dificil de seguir.

### `render()`: 694 linhas fazendo tres coisas ao mesmo tempo

Ela decidia o que ativar, registrava o que decidiu, e desenhava -- tudo
entrelacado, com as decisoes espalhadas por dez variaveis locais que iam sendo
corrigidas ao longo da funcao. Agora sao tres etapas separadas:

- `plan_frame()` devolve um `FramePlan` com o estado do depth e os seis
  interruptores de passe. Decidir virou uma coisa so, num lugar so.
- `log_frame_plan()` recebe o plano pronto e registra. Antes cada bloco de
  decisao carregava seu proprio `if` de log no meio.
- `compose_output()` le o plano e desenha. A cadeia de seis `else if` virou
  seis retornos antecipados, cada um chamando um passe nomeado.

`render()` tem hoje 17 linhas e le como um roteiro.

A leitura do depth, que era o trecho mais profundo do arquivo (quatro niveis de
`if` encaixados para decidir se o depth ainda esta vivo), virou tres funcoes
nomeadas -- `note_depth_active`, `note_depth_idle`, `depth_is_safe_for_scene` --
cada uma com um nivel.

O passe visual aparecia identico em tres ramos e o passe SSAO em tres tambem;
viraram `draw_visual_pass` e `draw_ssao_pass`, cada um chamado onde era copiado.

### `compile_shaders()`: 264 -> 38 linhas

Eram cinco blocos de compilacao e quatro de criacao, quase identicos, cada um
com seu proprio tratamento de erro e liberacao. `compile_shader_blob` e
`create_optional_pixel_shader` absorveram o padrao; o que sobra sao as chamadas,
uma por shader. As mensagens de log continuam identicas, incluindo a do bloom
que interpola o nome do entry point.

### `initialize_pipeline()` e `ensure_frame_resources()`

Os cinco constant buffers eram cinco blocos iguais variando so o `sizeof` e o
membro de destino; viraram uma tabela com ponteiro-para-membro. O trio
textura + SRV + RTV de `ensure_frame_resources` aparecia duas vezes, para as
texturas visual e espacial; virou `create_intermediate_target`.

`release_frame_resources` e a liberacao feita ao recriar os recursos
compartilhavam oito linhas mas nao sao a mesma coisa -- a primeira nao solta os
recursos temporais. A parte comum virou `release_scene_textures` e a diferenca
ficou visivel em vez de escondida em duas listas parecidas.

### Verificacao

Build e `validate.sh` verdes apos cada uma das quatro etapas, nao so no fim. Os
onze testes passam. Os literais que o `validate.sh` exige deste arquivo --
incluindo `scene_observer_.observe(device_, context_, scene_texture_);` e
`update_condition_adaptation();`, que fixam o ponto de medicao pre-grade --
foram conferidos antes de comecar e continuam intactos.

## Pacote 0.19.2 - 2026-09-10

Refatoracao de `src/dxgi_proxy.cpp`, pedida pelo usuario. Sem mudanca de
comportamento.

### Aninhamento

O problema estava concentrado em `graphics_worker`: um laco de repeticao com um
bloco condicional dentro, contendo outro laco, contendo um `if/else`. Quatro
niveis.

| | antes | depois |
| --- | --- | --- |
| maior funcao | 55 linhas | **24** |
| aninhamento de chaves | 6 | 4 |
| `if` mais profundo | indentacao 16 | **indentacao 8** |
| `if` dentro de outro `if` | 3 | **0** |

A contagem de `if` continua 11, e isso e proposital: nenhum deles sumiu, todos
viraram guarda plana ou ramo unico dentro de um laco. O arquivo passou de 144
para 188 linhas, porque constantes ganharam nome e a espera pelo overlay virou
funcao propria. Mais linhas, muito menos profundidade.

`CreateDXGIFactory`, `CreateDXGIFactory1` e `CreateDXGIFactory2` eram tres
copias do mesmo corpo com assinaturas diferentes; passaram a chamar um
`forward_to_system_dxgi` com encaminhamento perfeito. A sequencia de auditoria
pos-instalacao virou tabela, com o campo de espera nomeado
`delay_since_previous_milliseconds` -- os rotulos sao acumulados (500, 2000,
5000) e as esperas sao incrementos (500, 1500, 3000), e o nome do campo e o que
torna esse pareamento legivel sem comentario.

### A logica sutil saiu para onde um teste alcanca

`src/overlay_watch.hpp` passa a conter a maquina de estados que espera o overlay
da Steam **estabilizar** antes de instalar os hooks. Ela nao verifica apenas se
o modulo existe: exige o **mesmo endereco** por quatro amostras seguidas, porque
um overlay recarregado durante a espera reaparece em outro endereco e instalar
hooks nesse instante e justamente o que se quer evitar.

Isso vivia dentro de `dxgi_proxy.cpp`, que nao compila fora do Windows e
portanto nao tinha teste possivel. Agora e um cabecalho que so precisa de
`const void*`, e `tests/overlay_watch_test.cpp` cobre oito casos: overlay que
nunca aparece, que aparece e fica, que troca de endereco no meio, que some e
volta, que oscila entre dois enderecos, e o limiar exato de amostras.

Verificado que o teste cai quando a comparacao de endereco e trocada por um
mero teste de nao-nulo. `validate.sh` recusa o build nesse caso.

## Pacote 0.19.1 - 2026-09-10

Refatoracao de `src/config.cpp`, pedida pelo usuario, mais um defeito que ela
encontrou no caminho.

### O que mudou na forma

| | antes | depois |
| --- | --- | --- |
| linhas | 1096 | 912 |
| `else if` | 49 | **0** (o unico que resta esta num comentario) |
| `if (` | 113 | 21 |
| indentacao maxima | 16 | 12 |
| maior funcao | 193 linhas | 78 |

A leitura passou a ser dirigida por **tabela**. Antes cada parametro exigia
tocar em cinco lugares: o campo em `CalibrationStack`, o campo em `Settings`, um
ramo de `else if` na funcao de atribuicao da secao, uma linha na copia
stack -> Settings, e um clamp escrito a mao. Agora e **uma linha**:

```cpp
{"rain_temperature", &Settings::condition_rain_temperature},
```

Tres mudancas estruturais sustentam isso:

- **Uma tabela por familia, com ponteiro-para-membro.** `kGradeFields` liga o
  nome no arquivo ao campo em `Settings` e ao campo na camada, e as tres
  leitoras -- parse, copia da base, soma dos deltas -- percorrem a MESMA tabela.
  Nao ha como uma saber de um campo que a outra ignora.
- **`kSections` substitui o `enum Section`**, a cadeia que traduzia nome em
  enum e a cadeia que traduzia enum em funcao. Eram tres lugares para acertar,
  e a 0.19.0 chegou a esquecer um `continue` na segunda delas.
- **`CalibrationStack` parou de duplicar `Settings`.** So as tres camadas de cor
  precisam de armazenamento proprio, porque se somam; todo o resto e lido direto
  no formato final. Sairam 55 declaracoes repetidas e a segunda coluna de cada
  tabela.

Os clamps viraram `kLimits`, e os pares que precisam ficar em ordem crescente
viraram `kOrderedPairs` -- inclusive as bandas da adaptacao, onde uma inversao
devolveria a classe dura que a 0.19.0 existe para nao ter.

### O defeito que a refatoracao encontrou

`reference_base()` tinha `exposure = -0.09f` enquanto o cfg entregue ja dizia
`-0.0488697`. A 0.18.2 moveu os +0,0411 EV do balanco de branco para a exposicao
base, mudou o arquivo e **nao mudou o default interno**. Quem perdesse o cfg
rodava 0,041 EV mais escuro que a calibracao aprovada, e nada acusava porque o
`validate.sh` fazia grep de cada lado separadamente.

Corrigido, e agora ha teste.

### `config.cpp` passou a ter teste

`tests/config_load_test.cpp` compila o proprio `config.cpp` com um substituto
minimo de `<windows.h>` e exercita o binario de verdade: doze grupos cobrindo a
soma das camadas, o sufixo `_delta`, a forma escalar `black_lift` da 0.14.0,
secao e chave desconhecidas, os limites, banda invertida, comentarios e espacos,
`enabled` por modulo, as tres formas de booleano, e cfg vazio.

O primeiro grupo e o que impede a volta do defeito acima: **os defaults internos
e o cfg entregue tem que produzir o mesmo perfil**. Verificado que o teste cai
quando a `exposure` volta para -0,09.

### Sem comentarios no codigo

A pedido do usuario, todo comentario saiu de `src/`, `tests/` e `shaders/`:
**1089 linhas, zero comentarios restantes**. O fonte caiu de 10.353 para 9.264
linhas.

A justificativa medida de cada decisao -- a derivacao do `black_lift`, o limiar
do bloom, a troca do contraste afim pela potencia, os limiares de condicao --
continua inteira em `references/*.md` e neste CHANGELOG, que e onde ja morava e
onde da para procurar. O `.cfg` nao foi tocado: os comentarios de la sao a
documentacao que o usuario le para ajustar as ancoras.

Verificado antes de apagar que nenhuma guarda do `validate.sh` dependia de
texto de comentario. A unica que citava um -- a que proibe a reta de contraste
da 0.17.0 -- ja passava `sed 's|//.*||'` antes do grep, entao a limpeza a
deixou mais estrita, nao quebrada.

### Como a equivalencia foi verificada

Antes de mexer, os 132 campos de `Settings` foram despejados para arquivo, em
dois cenarios -- defaults internos e parse do cfg entregue. Depois da
refatoracao, o mesmo despejo: **identico nos 131 campos**, com a unica diferenca
sendo a `exposure` corrigida de proposito. O mesmo despejo foi repetido depois
da remocao dos comentarios: identico nos 132.

## Pacote 0.19.0 - 2026-09-10

**A cor passa a seguir a condicao.** Ate a 0.18.2 o perfil era unico --
`temperature=6400`, `tint=0.500` -- chovendo ou com sol. Nao era descuido: era
a media de condicoes que nao se parecem, e era a origem do esverdeado constante
de que o usuario reclamou duas vezes.

### Os limiares sao medidos, e sao do ETS2

386 amostras em quatro sessoes, agrupadas por **duracao**: nenhuma condicao de
tempo dura 30 segundos e nenhum artefato dura dez minutos, entao um bloco
sustentado se rotula sozinho.

| condicao        |   n | min |   ceu R/B (p10-p90)   | mediana | saturacao (p10-p90)  |
| --------------- | --- | --- | --------------------- | ------- | -------------------- |
| sol / ceu limpo | 216 | 108 | 0,885 (0,770 a 1,075) |  57,1   | 0,240 (0,198 a 0,302)|
| nublado / chuva |  46 |  23 | 1,001 (0,988 a 1,004) |  76,8   | 0,064 (0,057 a 0,075)|
| noite / escuro  |  41 |  20 | 0,957 (0,875 a 1,190) |   3,0   | 0,077 (0,058 a 0,104)|

Os dois blocos de chuva, de 9 e 14 minutos em sessoes separadas por seis dias,
devolveram `ceu R/B` 1,001 e 1,000 e `saturacao` 0,065 e 0,064 -- reproducao
quase exata. **O usuario confirmou que eram chuva.**

A **saturacao** separa sol de chuva sem sobreposicao alguma: o vao entre 0,075
e 0,198 e maior que as duas faixas somadas. A **mediana** separa noite de
chuva, 3,0 contra 76,8. Sao essas duas que decidem.

### O cuidado que decidiu o desenho

`ceu R/B` da chuva (1,001) e **maior** que o do ceu limpo (0,885): a regiao de
ceu mede a cor do ceu, e ceu limpo e azul. A feature detecta bem e seria um
alvo pessimo -- ligar `temperature` proporcionalmente a ela deixaria o sol frio
e a chuva quente, o oposto do pedido. Por isso a cor vem de **ancoras por
condicao**, nunca de realimentacao da feature.

As ancoras sao escolha de look e moram no cfg. Os defaults sao os valores
fotograficos de sempre: 5900 K no sol (luz solar direta e ~5500 K), 7500 K na
chuva (encoberto e sombra), 7000 K a noite. `tint` cai de 0,500 para
0,44 / 0,30 / 0,34, com a chuva no menor de todos para ler como azul e nao
como verde-azulado.

### Nenhuma classe dura, e agora e medido

Aplicar limiar duro amostra a amostra troca de classe **16 vezes por hora** em
jogo, e suavizar satura em 5 -- cada troca seria um salto de cor visivel. Dois
eixos com `smoothstep` e pesos que somam 1 fazem uma amostra no meio do caminho
sair no meio do caminho. `tests/scene_conditions_test.cpp` varre a saturacao em
passos de 0,001 e exige que a temperatura nunca de degrau; trocar o smoothstep
por um `if` derruba o teste.

Suavizacao exponencial de **180 s** sobre as features, em tempo e nao em
amostras. E o meio da regiao chata medida pelo erro de previsao causal (minimo
em 1,5 min numa sessao e 4 min noutra). A banda dia/noite ficou em 3-30 e nao
colada no vao medido: com 6-20 a reproducao das 386 amostras chegou a andar
1074 K em 30 s, porque o `smoothstep` e ingreme no meio; com 3-30 o pior caso
cai para 609 K.

Reproduzindo as amostras reais pelo detector compilado, os dois blocos de chuva
dao peso 0,925 e 0,892 e o bloco de sol de 61 minutos da 0,995.

### CORRECAO: a porta de jogo descartava a chuva

A porta proposta na 0.18.1 era `p90-p10 > 20` **e** `saturacao > 0,09`. Nestas
sessoes ela descartou 117 de 386 amostras, e **111 cairam so pela saturacao** --
com mediana 48, faixa 70 e media 52, ou seja cena de jogo bem iluminada.
Saturacao baixa nao e quadro invalido: **e o que encoberto e chuva parecem**. A
porta descartava 57 das 60 amostras de chuva.

A porta correta usa **so estrutura**, e derruba 6 de 386. `validate.sh` recusa
o build se `saturacao` voltar a aparecer nela.

### O que nao mudou

Com `enabled=false` a saida e identica a 0.18.2, e o mesmo vale para ancoras
iguais entre si -- ha teste para as duas coisas. Sem amostra valida ainda, o
perfil fixo do cfg continua valendo, entao a entrada no jogo comeca no que o
usuario aprovou. Um quadro preto ou de carregamento **segura** o estado em vez
de zera-lo: um quadro preto chegou a devolver `ceu R/B` = 1,596, o valor mais
quente de uma sessao inteira.

## Pacote 0.18.2 - 2026-09-04

O balanco de branco carregava exposicao, e a imagem nao muda.

No perfil aprovado (6400K, tint 0,50) o vetor de balanco e
0,9773/1,0500/0,9721, cuja luminancia Rec.709 e **1,028920**: +0,0411 EV que
ninguem pediu. Contra o `exposure=-0,030` do cfg, a exposicao efetiva era
**+0,0111 -- com o sinal trocado** em relacao ao que o arquivo e o log diziam.
O numero estava escondido desde a 0.1.2.

Enquanto `tint` era constante isso era um erro fixo, absorvido na calibracao.
A partir da 0.19.0 `tint` passa a se mover com o clima e o erro se move junto:
varrer tint de 0,0 a 1,0 desloca a imagem em 0,0803 EV, ou **5,7% de brilho**.
O eixo verde-magenta e o pior dos dois porque G pesa 0,7152 da luminancia. A
imagem clarearia ao ficar esverdeada e escureceria ao esfriar, sozinha. Cor que
muda brilho e o que se le como irreal.

`apply_temperature` passa a dividir o balanco pela propria luminancia, e os
+0,0411 EV foram para a exposicao base do cfg (-0,09 -> -0,0488697), onde da
para ler. Exposicao e balanco sao multiplicacao em linear e comutam, entao a
saida de hoje **nao muda**: medido sobre uma captura real do ETS2, 9 pixels de
11.059.200 diferem em 1 codigo (0,0001%), que e arredondamento de quantizacao.
A deriva ao varrer tint cai de 5,85% para 0,82%, e o que sobra e croma de
verdade passando pela curva de potencia e pelo ombro.

`tests/white_balance_test.cpp` fixa a propriedade em todo o dominio de
`temperature` e `tint`, incluindo que a normalizacao preserva as razoes entre
canais -- ela tira brilho, nao muda a cor. O teste ESPELHA o HLSL, porque a
funcao vive no shader; `validate.sh` amarra as duas copias com um grep na linha
da divisao e outro no numero medido. Uma linha nova de log passa a trazer o
balanco bruto, sua luminancia em EV, o normalizado e o ganho resultante, para
que isso nao possa se esconder de novo.

### De onde veio

Da avaliacao do repositorio `roimehrez/photorealism` (Screened Poisson
Equation, BMVC 2017) que o usuario trouxe. Registro completo em
`references/screened-poisson-avaliacao-0.18.2.md`, incluindo a recomendacao que
eu dei e que a medicao derrubou.

Resumo do que foi medido: o solve esparso global do repositorio e
**identico a `passa-baixa(graduado) + passa-alta(original)`** (verificado, erro
1e-13). Mas aplicado aqui ele apagaria o realce aprovado -- a curva tonal do
plugin so distorce estrutura em ±8%, e os 15,3% de ganho de gradiente sao
`sharpness` e `local_contrast`, que sao deliberados. E a distorcao de
luminancia que eu atribui ao balanco era, num controle com ganho acromatico de
mesma magnitude, 0,046% de matiz e o resto exposicao. Do paper ficou a pergunta
certa, nao o metodo.

## Pacote 0.18.1 - 2026-09-04

Correcao de tres defeitos que o primeiro log de jogo da 0.18.0 revelou. Dois
deles sao o **mesmo defeito**: uma tabela de formatos que so listava as
variantes tipadas e recusava o pai TYPELESS.

**O observador de cena passou a 0.18.0 inteira desligado.** O log de seis
sessoes tem 663.486 copias de `formato 90 ... nao suportados para leitura` e
zero linhas `Cena 0.18.0:`. O formato 90 e `B8G8R8A8_TYPELESS`, que e o que
`ensure_frame_resources` cria de proposito para poder pendurar uma SRV sRGB na
copia da cena -- ou seja, o observador recusava exatamente o formato do
caminho principal. `build.sh` compilou, `validate.sh` passou e o teste de
features passou, porque a guarda que existia fixava a *linha da chamada*, e a
chamada estava certa; o errado era a tabela logo depois dela, dentro do `.cpp`,
onde nenhum teste alcanca.

A tabela saiu para `src/scene_formats.hpp`, em `unsigned` para compilar no
teste sem `d3d11.h`, e `tests/scene_formats_test.cpp` fixa o caso que quebrou.
A piramide, a view e o staging passam a usar a variante **UNORM** resolvida --
nao `_SRGB`: com view UNORM o `GenerateMips` faz a media dos codigos de 8 bits
como numeros, com `_SRGB` ele decodifica para linear antes. As cinco
referencias foram medidas no espaco de codigo, entao `_SRGB` deslocaria
mediana e faixa contra a tabela sem que nada acusasse.

**O SSAO e o resolve temporal ficaram desligados nas tres sessoes mais
recentes.** Mesma causa, outro lugar: `depth_copy_formats` so listava os
formatos `D*`. O ETS2 declara o depth ora como 20 (`D32_FLOAT_S8X24_UINT`),
ora como 19 (`R32G8X24_TYPELESS`) -- o log tem os dois no mesmo dia. Nas tres
primeiras sessoes veio 20 e o SSAO subiu; nas tres ultimas veio 19 e cada
descoberta terminava em `Candidato depth incompativel com copia 0.9.1`.

O candidato recusado era o **melhor** dos dois: 19 vem com `bind_flags=0x48`,
ja legivel por shader, contra `0x40` do 20. Cada familia passa a entrar pelos
dois nomes; o destino da copia nao muda.

**A recusa era registrada uma vez por frame.** `ensure_resources` chamava
`release()` antes de comparar a assinatura da fonte, e o release zerava
justamente a assinatura -- entao a recusa era reavaliada e registrada em todo
frame. `resources_failed_` existia e nunca era lido. Resultado: 67 MB de log
numa unica sessao. A comparacao passou para antes do release e a falha guarda
a assinatura.

### O que o log confirmou que esta certo

- Custo do passe: media de 1,414 ms em 1.103 amostras de telemetria, pior
  media de janela 3,326 ms, zero amostras descartadas.
- Present/Present1: `state=nosso-hook-externo` em todas as fases de auditoria
  das seis sessoes, com o overlay da Steam estabilizado antes dos hooks.
- Nenhuma falha de shader, de recurso de frame ou de swap chain.

### O que continua em aberto

- **F12 da Steam.** As seis sessoes tem so a linha de registro do
  `ISteamScreenshots` e nenhuma captura. O diagnostico da 0.18.0 segue de pe e
  nao ha correcao aqui.
- **Nao ha ainda uma unica medida `Cena 0.18.0:` de dentro do ETS2.** A
  calibracao da 0.19.0 continua sem evidencia; esta versao e o que torna
  possivel colher a primeira.

## Pacote 0.18.0 - 2026-09-01

Observador de cena. **Este modulo nao altera um pixel** -- ele mede o frame
antes do grade e escreve quatro numeros no log. Detalhe em
`references/scene-observer-0.18.0.md`.

Ele existe por causa de um achado que muda a premissa da calibracao de cor.

**As cinco referencias nao sao cinco fotos da mesma coisa.** Foram abertas e
medidas aqui pela primeira vez, e sao cinco condicoes diferentes: encoberto
(23-33-14), anoitecer com farois (23-47-51), sol baixo com neblina (11-12-15 e
11-12-25) e dia claro (15-56-22). A calibracao de cor de hoje e a media das
cinco. O `tint` efetivo de 0,50 nao esta errado por descuido -- e o unico
numero possivel quando se tenta cobrir cinco condicoes com um so, e por isso
cai entre o alvo de dia claro e o de encoberto errando os dois.

Quatro features separam as cinco. Medidas pelo mesmo caminho do observador
(media de area ate ~80 px de largura):

| ref      | condicao    | ceu R/B | mediana | p90-p10 | saturacao |
| -------- | ----------- | ------- | ------- | ------- | --------- |
| 23-47-51 | anoitecer   | 0,915   | 13,1    | 55,0    | 0,421     |
| 23-33-14 | encoberto   | 0,967   | 21,2    | 131,2   | 0,338     |
| 11-12-25 | sol/neblina | 1,005   | 21,0    | 102,4   | 0,260     |
| 11-12-15 | sol/neblina | 1,000   | 30,3    | 95,2    | 0,212     |
| 15-56-22 | dia claro   | 0,938   | 43,9    | 157,7   | 0,183     |

Nenhuma separa sozinha e cada uma desempata um par que as outras confundem. O
par de condicoes diferentes mais proximo fica a 1,65 no espaco normalizado; o
par mais proximo de todos e `11-12-15 x 11-12-25`, a 1,07, que e a **mesma**
condicao dez segundos depois com a camera para outro lado. A dispersao dentro
da condicao e menor que a distancia entre condicoes, que e a propriedade
necessaria -- e a margem de 1,5x e apertada, o que ja decide que a adaptacao
tera de interpolar continuamente em vez de classificar em classe dura.

`tests/scene_features_test.cpp` guarda os dois numeros e inclui
`src/scene_features.hpp` direto, sem espelhar a implementacao.

Nesta versao:

- `src/scene_features.hpp`, novo: a matematica das features, sem D3D11, para
  poder ser testada;
- `src/scene_observer.{hpp,cpp}`, novo: `GenerateMips` sobre uma copia da cena,
  leitura assincrona com `D3D11_QUERY_EVENT` e `DONOTFLUSH`, dois slots de
  staging, amostra descartada em vez de esperar;
- a medicao acontece colada no `CopyResource(scene_texture_, back_buffer)`, e
  `tools/validate.sh` verifica essa linha: medir a saida fecharia uma
  realimentacao entre a cor e as features;
- `[module.scene_observer.0.18.0]` no cfg, ligado por padrao com
  `interval_frames=30` e `log_seconds=30`;
- **a linha `Perfil efetivo` do log ganhou `tint`, `highlight_rolloff` e os
  tres `black_lift`.** Estavam fora desde a 0.14.0, e sem eles nao havia como
  confirmar em runtime qual cor estava rodando -- o que custou uma sessao
  inteira de analise para descobrir que o verde vinha do `tint`.

Nao ha detector ainda, nao ha adaptacao ainda, e nenhuma das cinco referencias
tem chuva -- a camada que leva o nome `rain_overcast` continua sem alvo medido.

## Pacote 0.17.2 - 2026-09-01

O piso do preto de novo, porque **o estimador que fixou a 0.17.1 tinha vies**.
Detalhe em `references/tone-floor-0.17.2.md`.

A 0.17.1 foi confirmada em jogo com doze capturas no ETS2 e o defeito central
sumiu: o plato de crush nao existe mais, o cinza exato na sombra caiu de 72-90%
para 0,00-0,15% e nada estoura no teto nem com o sol no horizonte. Isso esta
registrado em `references/tone-floor-confirmation-0.17.1.md`.

O que esta versao corrige e o alvo, nao a implementacao.

**O estimador de cauda tem vies quando a imagem tem grao.** A media do 1% mais
escuro escolhe os pixels ordenando por LUMINANCIA, que pesa G em 0,7152, R em
0,2126 e B em 0,0722. Numa imagem com grao, o que faz um pixel entrar nessa
amostra e ruido negativo no canal G. So o G desce.

Medido por injecao, e nao por argumento: somando grao gaussiano de desvio 2,1
-- o das referencias -- as doze capturas limpas do plugin, a cauda se desloca
**-0,18 / -1,83 / +0,43 codigos**. O mesmo teste valida o estimador no caso
limpo: ele recupera o piso conhecido do plugin, que e o proprio `black_lift`,
dentro de 0,35 codigo.

As referencias tem grao de desvio 2,1 e as capturas 0,18. A 0.17.1 comparou os
dois com o mesmo estimador sem corrigir nada.

**A primeira simulacao desta versao pedia a correcao ao contrario.** Sobre os
numeros crus, o alvo dava `black_lift_g` **24% menor**. Corrigido o vies, da
**13% maior** -- 37 pontos percentuais de diferenca, e uma troca de sinal.

Alvo novo, mediana por canal das cinco referencias corrigidas: 4,78/7,84/7,82
em 255, contra 4,60/6,01/8,25 sem correcao.

**Os tres numeros nao sao a conversao direta do alvo.** O piso medido numa
captura e o lift mais o que a cena poe por cima, cerca de 0,6 codigo. Entao
eles saem de resolver o lift que POE a cauda no alvo: invertendo a etapa afim
do shader pixel a pixel nas doze capturas -- `out = lift + (1-lift)*c` e
exatamente inversivel -- e bisseccionando por canal.

    efetivo   0.001150/0.002192/0.002313  ->  0.001398/0.002480/0.002268
    base      0.000640/0.001767/0.001590  ->  0.001017/0.001982/0.001888
    piso      4/7/8 em 255                ->  5/8/7 em 255

Cada canal do piso novo cai dentro da faixa das cinco corrigidas: R de 2,86 a
7,64, G de 6,15 a 9,67, B de 6,03 a 10,78.

**`tests/tone_curve_test.cpp`** ganhou a regressao dos codigos novos e a nota
de que a razao do LIFT nao e a razao do PISO: a cena soma a mesma parcela nos
tres canais, o que puxa a razao do piso na direcao de 1,0. B/G da 0,915 no lift
e 0,997 no piso simulado. As duas faixas do teste diferem por isso, e nao por
folga arbitraria.

**O que nao mudou.** O shader nao foi tocado -- contraste em potencia e
`apply_black_lift` por canal continuam como saíram da 0.17.1. Falta grao na
sombra, ~7x abaixo da referencia ja descontado o downsample do gamescope, e
esse e o proximo item.

## Pacote 0.17.1 - 2026-08-31

O piso do preto, e **a medicao que mostrou que o culpado era o contraste**.
Detalhe em `references/tone-floor-0.17.1.md`.

Quatro capturas da 0.17.0 medidas contra as cinco referencias do ATS. Pelos
tres criterios do `grade_report.py` a 0.17.0 **passa**: p1 entre 7 e 8, dentro
da faixa 6-12; G como canal mais alto; `topo%` 0,00. Os dois defeitos maiores
estavam presentes assim mesmo.

**O piso nao era um piso, era um plato.** O 1% mais escuro das quatro capturas
saiu 8/8/8 e 9/9/9 -- exatos, nao aproximados. Abaixo de 12/255 sobravam 12 a
13 niveis distintos contra 24 a 31 nas referencias, o desvio-padrao era 0,70 a
1,19 contra 3,10 a 4,01, e **72 a 90% dos pixels escuros tinham os tres canais
identicos** contra 0,00 a 0,77% nas referencias. Era por isso que o interior da
cabine lia como plastico cinza chapado em vez de preto profundo.

**A causa nao era o `black_lift`.** Era
`max((color - pivot) * Contrast + pivot, 0.0)`: com `Contrast` acima de 1 a
reta manda todo valor abaixo de `pivot*(Contrast-1)/Contrast` para negativo e o
clamp junta o conjunto inteiro no mesmo zero. Com o perfil aprovado esse limiar
e 0,01178 na entrada do passo, ou 0,0147 na entrada da cadeia -- **32 em 255**,
a cabine inteira. O `black_lift` vinha depois e so escolhia QUAL valor a massa
esmagada receberia. Trocar so ele nao mudaria nada.

- **contraste em potencia**, `pivot * pow(max(color, 1e-6) / pivot, Contrast)`.
  Mesmo pivo, praticamente a mesma inclinacao perto dele, manda 0 para 0 em vez
  de para negativo e e monotonica em todo o dominio. O degrade de entrada 0-40
  em 255 devolve **30 niveis distintos em vez de 10**;
- **`black_lift` por canal.** O p1 da luma (8 a 11) tinha a magnitude certa e
  escondia a cor: o 1% mais escuro das referencias e 2,1/5,8/5,2 encoberto,
  1,6/5,7/5,1 crepusculo, 3,8/7,2/7,6 sol, 5,8/9,1/10,6 e 5,7/9,0/10,5
  neblina. R fica entre 29% e 64% de G nas cinco. Um lift escalar e acromatico
  por construcao, e `temperature`/`tint` nao alcancam isso -- os dois
  multiplicam a faixa inteira, e o topo ja estava certo (R/G 0,955-1,002 na
  referencia contra 0,945-0,958 na 0.17.0). As tres camadas estao **sempre
  somadas** -- nao ha deteccao de clima -- entao quem tem que cair no alvo e a
  soma: a base leva o piso de tempo claro (2,1/5,8/5,2) e `rain_overcast`
  completa ate a mediana por canal das cinco, **4/7/8**. Mirar a soma nas duas
  de neblina (5,8/9,1/10,6) poria o piso permanente no extremo da faixa medida
  em vez do centro;
- **`black_lift_r/g/b` no cfg**, com `_delta` nas duas camadas. A forma escalar
  `black_lift=` continua aceita e escreve os tres canais iguais -- ou seja,
  reproduz exatamente o piso acromatico que esta versao corrige;
- **o cbuffer continua com 96 bytes.** `float3 BlackLift` mais
  `HighlightRolloff` fecham uma linha de 16, e `Tint` desce para a linha do
  bloom;
- **`tests/tone_curve_test.cpp`** ganhou a regressao das duas coisas: que a
  forma afim junta 0,002 e 0,010 no mesmo zero e a potencia nao, e que o piso
  sai 2/6/5 na base e 4/7/8 efetivo, com R/G e B/G dentro da faixa das cinco.
  O contador de niveis distintos exige pelo menos 24, que e o piso do que as
  referencias mostram;
- **duas guardas novas em `validate.sh`** -- a linha do contraste em potencia e
  `black_lift_r < black_lift_g`. A segunda existe porque o modo de falha nao e
  alguem zerar o lift, e alguem reigualar os tres.

O que **nao** muda nesta versao, e que a mesma medicao mostrou faltar: o grao.
As referencias tem 0,95 a 1,14 niveis de ruido por canal em regioes claras e
lisas, e as capturas tem 0,16 a 0,32.

Uma correcao de processo: os tres criterios do `grade_report.py` passaram com
os dois defeitos presentes. `p1` nao distingue um piso com estrutura de um
plato no mesmo valor. Faltam ali niveis distintos abaixo de 12/255, fracao de
pixels escuros com R=G=B exato, e a razao R/G do piso.

## Pacote 0.17.0 - 2026-08-30

Bloom, **e a medicao que corrigiu a premissa da propria versao**. Detalhe em
`references/bloom-0.17.0.md`.

A justificativa original era que as referencias do ATS mostram glare. Medidas
depois que a estrutura ficou pronta, **elas nao tem bloom**: toda borda entre
muito claro e muito escuro esta nitida, o perfil de borda em resolucao cheia
mostra o lado escuro plano ate a transicao **sem cauda** -- a assinatura que
bloom deixaria -- os mostradores noturnos nao iluminam nada em volta, e `topo%`
e 0,00 nas cinco. A afirmacao de que havia "halacao na borda do para-brisa"
**estava errada**: foi observacao a olho nunca conferida nos pixels.

O que as referencias tem sao **raios de sol** -- estriados radiais saindo do
sol, projetados no teto escuro da cabine. Sao direcionais, e uma piramide
gaussiana nao produz aquilo. Isso e a 0.18.0, e reaproveita o bright-pass e a
piramide desta versao.

O modulo fica **ligado por decisao do usuario, com `intensity=0.02`** e a
ressalva escrita no cfg e guardada por `validate.sh`. Dos quatro parametros so
o limiar e medido: `0.85` em sRGB e o codigo 217, acima do p95 das cinco
referencias (117 a 212), o que faz o bloom pegar o disco do sol e o topo das
nuvens em vez do ceu.

- **`shaders/bloom.hlsl`**, tres entry points -- limiar de joelho suave na
  descida, box na reducao, tent na subida com blend aditivo. Sem vertex shader
  proprio: reusa o `VSMain`, como ssao e temporal ja faziam;
- **piramide de ate seis niveis, e nao um blur maior.** Nove taps a meia
  resolucao alcancam 0,007 da altura; o flare das referencias esta uma ordem de
  grandeza acima, e esticar os offsets deixa buracos que viram anel. `radius` e
  fracao da altura e vira contagem de niveis, o que faz o mesmo valor servir em
  1080p e em 4K;
- **a composicao acontece dentro do `PSMain`**, entre o sharpening e os
  controles tonais. A cadeia do frame ja tem cinco ramos e todos comecam pelo
  mesmo passe visual -- gerar a piramide antes do `if` e ler o resultado em
  `t1` faz o modulo valer para os cinco **sem criar ramo nenhum**;
- **`Insert` volta a sete posicoes**, a 6 mostrando o bloom isolado;
- **`tools/bloom_report.py`** -- mede o perfil radial em volta das fontes e
  devolve os tres parametros. Conferido contra alvos sinteticos: recupera 0,035
  como 0,038 e 0,094 como 0,100, vies de ~7% para cima;
- **`tests/bloom_curve_test.cpp`** prova o zero exato abaixo do joelho em cem
  pontos. Nao basta ser pequeno: se um pixel escuro contribui, todos
  contribuem, e o bloom vira nevoa cinza em vez de brilho em volta de fontes.

**Com `enabled=false` a imagem e identica a 0.16.0.** O `PSMain` ramifica na
constante, entao nada e somado. Os outros quatro entry points sairam
byte-identicos; so o `PSMain` mudou, de 308 para 319 instrucoes.

Uma correcao de processo encontrada ao provar as guardas novas: as capturas de
numero de linha do `validate.sh` usam `grep | head | cut`, e sob
`set -euo pipefail` um grep que nao acha nada **derruba o script sem imprimir
nada**. As guardas de ordem morriam caladas -- inclusive a do `black_lift`, que
estava assim desde a 0.14.0 e ninguem tinha notado porque a linha que ela
procura nunca faltou. As cinco capturas ganharam `|| true`, e as sete guardas
novas foram provadas falhando com a propria mensagem.

## Pacote 0.16.0 - 2026-08-30

Remocao completa do RTGI, a pedido do usuario e pela mesma razao medida na
0.14.0: nenhuma das cinco referencias que definem o alvo visual mostra efeito
que exija tracado de raios. A luz de preenchimento da cabine e uniforme e **sem
sangramento de cor** -- nao ha verde da grama no painel nem vermelho do
caminhao a frente -- e o brilho dos mostradores ao anoitecer nao ilumina nada em
volta. O modulo estava desligado desde entao.

Com a 0.15.0, o plugin perdeu **17.000 linhas em duas versoes** e ficou com o
que o visual realmente usa: curva de tom, coloracao, iluminacao, TAA/AA nativo
e SSAO.

- **1.252 linhas apagadas** em tres arquivos -- `shaders/rtgi.hlsl` com seus
  tres entry points, `src/rtgi_config.hpp` e `tests/rtgi_config_test.cpp` --
  mais 377 referencias em `postprocess.cpp`, 47 em `config.cpp` e a secao
  `[module.rtgi.0.12.0]` do cfg;
- **a cadeia do frame voltou a forma pre-0.13.2.** `grading_source` existia so
  porque o passe de composicao podia substitui-lo; sem RTGI ele era sempre
  `scene_view_`, e os tres `PSSetShaderResources` voltaram a ler a cena
  diretamente;
- **`Page Up` e `Page Down` deixaram de existir.** Eram do RTGI e de mais nada.
  `Insert` passa de sete para **seis** posicoes, terminando na mascara de
  visibilidade do SSAO;
- **`references/rtgi-*.md` ficam** -- os cinco. Sao registro retrospectivo de
  medicao, diferente do `FSR_ROADMAP.md`, que era plano futuro e saiu junto com
  o codigo na 0.15.0. Um deles carrega a conta que redirecionou o projeto;
- **guarda de nao-retorno** para o codigo e para as duas teclas, no molde da
  0.15.0. O padrao e so `rtgi`, sem termo generico: na 0.15.0 um `easu` no
  padrao casou com "m**easu**re" e derrubou `grade_report.py`.

**Nenhum pixel muda.** Os cinco entry points restantes sairam byte-identicos a
0.15.0, e o modulo estava desligado.

Uma correcao de processo que a 0.15.0 tinha deixado passar: as guardas da curva
de tom da 0.14.0 estavam posicionadas logo depois de `max_indirect_luma`, que
era **chave do RTGI** -- e sairam junto com o bloco dele, em silencio. Foram
recuperadas e movidas para junto dos outros pinos do cfg, com um comentario
explicando por que o lugar importa. Guarda misturada com modulo alheio morre
com o modulo alheio.

**A faixa escura horizontal fecha aqui, e nao era o SSAO.** Testada em jogo
nesta versao: nao existe mais. O usuario esclareceu que ela so aparecia com o
RTGI ligado, na parte inferior da tela, onde o tracado nao alcancava -- ou
seja, era a fronteira entre a regiao que recebia GI somado e a que nao recebia
nada. Com o modulo fora, a fronteira nao tem como existir. A hipotese do SSAO
registrada na 0.14.0 estava errada, e o item sai do roteiro.

## Pacote 0.15.0 - 2026-08-30

Remocao completa do modulo FSR/AA auxiliar, a pedido do usuario. A 0.14.0
acertou o alvo visual medido, e a conclusao foi que o foto-realismo pretendido
sai de tonemap, coloracao, iluminacao, TAA/AA nativo e SSAO -- nenhum deles
passa pelo FSR.

O proprio log sustentava isso ha versoes: `replacement=0 dispatch=0`. O modulo
nunca substituiu um draw nem despachou um passe.

- **5.833 linhas apagadas** em 26 arquivos: os 18 de `src/fsr_*` mais a ABI e o
  `.def`, `shaders/fsr1.hlsl`, `third_party/fidelityfx-fsr/`, os tres testes de
  FSR e o `FSR_ROADMAP.md`. O historico do git preserva tudo, e a branch
  `fsr-0.7.2-tiles` continua intacta;
- **oito hooks de vtable removidos**, e este e o ganho que nao aparece no
  diff: `PSSetShaderResources`, `RSSetState`, `RSSetViewports`,
  `RSSetScissorRects`, `Draw`, `DrawIndexed`, `DrawIndexedInstanced` e
  `DrawInstanced` existiam **so** para alimentar o FSR. Cada corpo era
  `if (fsr_processing_enabled()) observe_fsr_*(...)`, e o ETS2 emite milhares
  de draws por frame -- cada um pagava uma indirecao, um load atomico e um
  branch para alimentar um modulo que nao fazia nada;
- **os hooks de `OMSetRenderTargets` e da variante com UAVs ficaram**: a
  descoberta de depth vive neles, e sem ela SSAO, resolve temporal e RTGI
  perdem a fonte. Perderam so a chamada `observe_fsr_color_targets`;
- **`photorealism-fsr.dll` deixa de existir.** A instalacao passa de tres
  arquivos para dois, e o nome do pacote perde o segmento de FSR:
  `photorealism-plugin-0.15.0-ets2-ats-1.60-proton`. `dxgi.dll` encolheu 7 KB;
- **`native_aa` nao e FSR e ficou.** Ele administra o `r_aa` do jogo, que e o
  TAA que o usuario quer manter -- o nome do pacote e que associava AA a FSR,
  por historico;
- **`validate.sh` ganhou duas guardas de nao-retorno**, uma para o codigo e
  outra para os oito hooks. Uma remocao sem guarda volta sozinha na primeira
  vez que alguem colar um trecho antigo. A primeira versao da guarda usava
  `easu|rcas` no padrao e derrubava `grade_report.py`, que contem "m**easu**re";
- **`README.md`**: as tres secoes de estado ainda descreviam a 0.11.3 e eram
  quase inteiramente narrativa de FSR. Foram substituidas por uma secao atual.

**Nenhum pixel muda.** Os sete entry points de shader sairam byte-identicos a
0.14.0, e o modulo removido nunca executou nada. O que muda e o que deixa de
ser executado a cada chamada de desenho.

O RTGI continua no codigo, desligado. Sai numa versao propria: ele tem 377
referencias dentro de `postprocess.cpp`, entrelacado com a cadeia que alimenta
o grading e o SSAO, enquanto o FSR tinha 33 em dois arquivos.

## Pacote 0.14.0 + Photorealism FSR/AA 0.7.1 - 2026-08-30

Mudanca de direcao, motivada por medicao. O usuario mostrou cinco capturas do
ATS com um shader de terceiros como alvo; os histogramas foram medidos em vez
de julgados no olho, e o resultado inverte a premissa das tres versoes
anteriores. Detalhe em `references/tone-curve-0.14.0.md`.

| | p1 | mediana | canal mais alto |
|---|---|---|---|
| Referencia (4 imagens) | **8–11** | 11–40 | **G** |
| Plugin 0.13.3 (3 imagens) | **0** | 47–70 | **B** |

- **curva de tom em `photorealism.hlsl`, que nao tinha nenhuma.** O shader
  terminava em `saturate()`: os altos cortavam retos e os baixos esmagavam em
  zero. Entram `apply_black_lift` e `apply_highlight_rolloff`;
- **`black_lift=0.0027`.** Em linear leva o preto a `0.0027 * 12.92 = 0.0349`
  em sRGB, ou 8,9 em 255 -- exatamente a faixa medida nas quatro referencias.
  E por isso que o painel do plugin virava massa preta enquanto o da
  referencia, **mais escuro na mediana**, deixava ler cada manometro. A
  0.13.2.1 e a 0.13.3 tentaram resolver isso somando luz, e o alvo tem a cabine
  mais escura que a nossa: o problema nunca foi falta de luz;
- **o lift e a ultima coisa antes do encode**, depois da vignette. Antes dela
  os cantos escureceriam abaixo do piso. `validate.sh` guarda a ordem por
  numero de linha;
- **eixo de tint no balanco de branco.** `apply_temperature` so trocava R
  contra B e nunca tocava em G, e as quatro referencias tem G como canal mais
  alto -- o alvo era inalcancavel por qualquer combinacao dos valores
  existentes;
- **`blacks` da base de `-0.01` para `0.05`.** Somado aos dois deltas valia
  `-0.06` e empurrava os pretos para baixo, contra o alvo. Agora soma zero;
- **`tools/grade_report.py`.** Toda a serie 0.13.x foi calibrada no olho, e foi
  assim que um efeito de cinco niveis em 255 sobreviveu tres versoes. A medida
  vira ferramenta do repositorio;
- **o RTGI para, desligado.** Nenhuma das cinco referencias mostra efeito que
  exija tracado de raios: a luz de preenchimento da cabine e uniforme e sem
  sangramento de cor, e o brilho dos mostradores ao anoitecer nao ilumina nada
  em volta. O modulo nao esta descartado -- esta na direcao errada para este
  alvo, e hoje empurra contra ele somando ruido e levantando meios-tons;
- **`color_rejection` de `0.05` para `0.5`.** O `0.05` da 0.13.3 era erro meu: o
  termo compara o frame atual com o historico, e num buffer de GI a diferenca
  entre os dois e o ruido que a acumulacao existe para eliminar. A historia era
  descartada todo frame, que e por que o granulado continuou visivel;
- **`validate.sh`: o hash do shader visual passou para depois das guardas de
  curva de tom**, pela mesma razao que o do cfg na 0.13.3 -- vindo antes, ele
  saia com "Shader visual aprovado foi alterado" e as guardas nomeadas nunca
  falavam. Quatro pinos nus viraram guardas com mensagem;
- **bytecode**: so `PSMain` mudou (272 para 309 linhas). `VSMain` e os seis
  outros entry points sairam byte-identicos ao HEAD anterior.

Nao mudaram, de proposito: exposicao, contraste, saturacao e vibrance. A 0.13.2
e a 0.13.3 moveram varias coisas de uma vez e nenhum A/B ficou interpretavel.

Pendente: a faixa escura horizontal a ~84% da altura, presente desde a
0.13.2.1. `PSRtgiCompose` so soma e a vignette e radial, entao nao sao eles; a
hipotese principal e o SSAO, cuja calibracao foi aprovada sobre uma cascata de
sombra antes de a 0.13.0 corrigir a elegibilidade do depth. Depende de teste em
jogo.

## Pacote 0.13.3 + Photorealism FSR/AA 0.7.1 - 2026-08-30

Acumulacao temporal do GI, e a recalibracao de escala que torna a versao
visivel. Detalhe em `references/rtgi-temporal-0.13.3.md`.

- **`PSRtgiTemporal`**, terceiro entry point de `rtgi.hlsl`, roda na resolucao
  do RTGI entre a marcha e a composicao. Os quatro parametros marcados
  `INERTE ate a 0.13.3` desde a 0.12.0 finalmente chegam ao cbuffer:
  `history_weight` como teto, e `depth_rejection`, `normal_rejection` e
  `color_rejection` como confiancas **multiplicadas** -- qualquer uma delas
  dizendo "nao e a mesma superficie" descarta a historia inteira. Media
  deixaria duas encobrirem a terceira, que e a quina do painel contra o
  para-brisa: mesma distancia, mesma cor, outra normal;
- **`normal_rejection` passa a existir de fato.** E a rejeicao que a 0.13.2 nao
  tinha. `reconstruct_view_normal` ja recebia as cinco amostras de depth por
  parametro, entao serviu as duas texturas sem alteracao;
- **o hash por raio saiu do `sin`.** Era `frac(sin(dot(seed, ...)))` com o
  frame no seed, e em fp32 o `sin` de argumento grande perde exatamente os bits
  que o `frac` usa -- o hash empobrecia ao longo dos minutos em que a
  acumulacao deveria estar somando amostras novas. Virou PCG em inteiro;
- **rotacao por angulo dourado e estratificacao.** O azimute gira
  `frac(frame * 0.38196601)` por frame e `random.x` passa a percorrer faixas de
  `1/ray_count`. Sem as duas, acumular converge para a media de amostras mal
  distribuidas;
- **`gi_intensity` sobe de `0.15` para `0.6`.** As oito capturas com a 0.13.2.1
  nao mostraram diferenca no interior, e a conta explica sem A/B: `sky_ambient`
  e `gi_intensity` se empilham, o teto do que um raio escapado somava era
  `0,0375` linear e o raio tipico no painel ficava em `~0,008` -- cerca de **5
  niveis em 255** sobre uma superficie ja escura. O conserto da 0.13.2.1
  existia, estava correto, e era invisivel;
- **`color_rejection` cai de `0.15` para `0.05`.** O `0.15` veio do resolve
  temporal, que opera sobre a imagem final; num buffer de GI com valores na
  casa de 0,05 um limiar absoluto de 0,15 nunca dispararia -- aceitaria
  historia sempre, que e ghosting;
- **limite registrado: nao ha reprojecao.** Sem matrizes de camera, a historia
  e lida no mesmo uv. Para o interior isso e exato, porque painel, volante e
  bancos nao se movem em relacao a camera -- e o interior e o alvo. Em curva o
  exterior volta ao ruido da 0.13.2.1, e isso e o desenho funcionando;
- **`validate.sh`: tres defeitos de guarda corrigidos.** O hash do cfg rodava
  **antes** de todas as guardas por chave e as deixava mudas -- qualquer edicao
  saia com "Configuracao consolidada foi alterada", inclusive a guarda de
  `sky_ambient` da 0.13.2.1. O hash passou para depois. A guarda de "parametro
  zerado" estava atras dos pinos de valor exato e era inalcancavel. E duas
  guardas novas eram `grep` nus sob `set -e`, que derrubam a validacao sem
  dizer nada;
- **bytecode**: os cinco entry points aprovados sairam byte-identicos ao HEAD
  anterior (2367 linhas). `PSRtgi` foi de 682 para 722 linhas, `PSRtgiTemporal`
  tem 426, e `PSRtgiCompose` mudou uma unica linha -- `cb0[5]` virou `cb0[7]`,
  pelos campos novos no cbuffer compartilhado.

## Pacote 0.13.2.1 + Photorealism FSR/AA 0.7.1 - 2026-08-30

Correcao da 0.13.2. A composicao funcionou, mas a cabine continuou preta, e o
teste em jogo mostrou por que: um tunel de concreto branco iluminado em volta
inteira, com o painel preto chapado e **sem nem granulado**. Ruido ausente onde
deveria haver ruido nao e denoise faltando -- e sinal ausente.

- **os raios que escapam deixam de devolver preto.** `march_ray` tem quatro
  desfechos e so um e acerto real. Os outros tres -- saiu da tela, ceu de
  verdade, e escape (passos esgotados ou plano proximo cruzado) -- agora
  devolvem o mesmo termo, `ambient_escape(direction)`. Ate a 0.13.2 o terceiro
  devolvia zero duro, e com `sky_ambient=0.0` no cfg os tres devolviam preto:
  o shader respondia breu a todo "nao sei";
- **`sky_ambient` passa de `0.0` para `0.25`**, no cfg e no fallback interno.
  Nao e a radiancia de um ceu; e a de uma direcao **desconhecida**, e em jogo a
  maioria delas esta parcialmente ocluida -- cabine, tunel, viaduto, vao entre
  predios. Um quarto de um ceu encoberto tipico e o lado conservador dessa
  conta;
- **por que isso custava a cabine inteira.** `reconstruct_view_normal` forca
  toda normal a apontar para a camera, entao o hemisferio de amostragem do
  painel e o cone entre o painel e o olho do motorista: ar vazio. Os raios
  tipicos andam para tras e cruzam o plano proximo; os rasantes sobem em
  direcao ao para-brisa e voam a frente da estrada, dezenas de metros adiante,
  esgotando os doze passos. Nenhum acerta. Os quatro devolviam exatamente
  `0.0`, e quatro zeros tem media exatamente zero -- preto liso, sem o
  granulado que teria denunciado o problema um mes antes;
- **o que a 0.13.2 realmente entregou, e o que nao.** A marcha geometrica
  consertou um ponto cego de amostragem que era real, mas ele nao era a unica
  coisa entre o RTGI e o interior. O criterio de aceite escrito no plano --
  "painel e bancos deixando de ser preto chapado" -- nao tinha como ser
  atingido naquela versao;
- **limite que continua, e agora esta medido.** Isto da a cabine um piso de
  ambiente modulado pela direcao do raio, que e o efeito de penumbra. Nao da
  color bleeding de verdade do exterior para dentro: a estrada esta *atras* do
  painel em view-space, fora do hemisferio dele, e nenhum ajuste de parametro
  alcanca isso em screen-space puro;
- regressoes travadas em `tools/validate.sh`: `sky_ambient=0.25` pinado,
  `sky_ambient=0.0` barrado por nome, `ambient_escape` exigido em `rtgi.hlsl` e
  a contagem dos tres desfechos de escape verificada. Em
  `tests/rtgi_config_test.cpp`, `clamped_defaults.sky_ambient > 0.0f`.

## Pacote 0.13.2 + Photorealism FSR/AA 0.7.1 - 2026-08-29

- **o RTGI passa a alterar a imagem do jogo.** Ate a 0.12.1 o buffer de GI era
  preenchido e so as debug views o liam. `PSRtgiCompose`, segundo entry point
  de `rtgi.hlsl`, soma o GI a cor de cena em espaco linear com `gi_intensity`,
  antes do grading -- para que exposure, contraste e LUT alcancem tambem a luz
  indireta, que era o requisito declarado no documento da tecnica;
- **a marcha virou geometrica, e sem isso a composicao nao alcancaria a
  cabine.** O passo fixo valia `(15.0-0.5)/12 = 1,21 m` e o laco somava antes
  de amostrar, entao nada era amostrado entre 0,5 e 1,71 m -- banco (~0,5 m),
  painel e GPS (~0,7 m) e para-brisa (~1 m) ficavam num ponto cego e um raio
  saindo do painel pulava a cabine inteira. Com razao geometrica e
  `range_min=0.10`, seis das doze amostras caem no interior e o exterior
  continua alcancando 15 m, com o mesmo numero de passos;
- `hit_thickness` deixa de ser espessura fixa e vira **teto**: a ambiguidade
  que a marcha introduz e o proprio comprimento do passo, entao a espessura
  usada e `min(passo, hit_thickness)`. Perto da ~0,05 m e impede a luz de vazar
  pelo painel; longe o teto impede que uma fatia de 5 m aceite qualquer coisa;
- **`ray_count`, `gi_intensity` e `max_indirect_luma` deixam de ser inertes.**
  Quatro raios somados e divididos; `max_indirect_luma` aplicado **por raio**,
  antes da media, porque e rejeicao de firefly -- depois da media o estrago de
  uma amostra estourada ja estaria diluido em todos;
- amostragem passa a ser cosine-weighted e o `dot(N, dir)` explicito **sai**:
  com o peso ja no PDF ele viraria `cos²` e escureceria os bounces rasantes,
  que sao os que carregam o color bleeding de parede e de painel;
- `rtgi_step_size` da lugar a `rtgi_step_ratio`, `rtgi_sample_distance` e
  `rtgi_samples_within` em `src/rtgi_config.hpp`. A ultima transforma a
  cobertura da cabine em invariante testada:
  `rtgi_samples_within(0.10f, 15.0f, 12, 1.5f) >= 4` devolve 3 e falha com o
  `range_min=0.5` antigo;
- as debug views `rays` e `hit_distance` passam a mostrar o primeiro raio --
  sao diagnostico por raio; `raw_gi` e `confidence` mostram o acumulado;
- **`photorealism.hlsl` nao foi tocado.** A composicao e passe proprio em vez
  de mais um trecho do grading, e os quatro shaders aprovados sairam
  byte-identicos ao HEAD: 2367 linhas de disassembly iguais dos dois lados,
  verificado com `tools/shader_check.sh`;
- custo: `PSRtgi` de 591 para 688 linhas de bytecode (o laco de raios e
  dinamico, entao o que multiplica e a execucao) e `PSRtgiCompose` com 60. O
  modulo continua nascendo desligado: quatro raios sem denoise ainda cintilam,
  e e a 0.13.3 e a 0.13.4 que resolvem isso.

## Pacote 0.13.1 + Photorealism FSR/AA 0.7.1 - 2026-08-28

- **Page Down era engolido em silencio.** Ate a 0.13.0 ele so tinha efeito com
  o Insert na posicao 6; fora dela `key_pressed_once` consumia a tecla e nada
  acontecia -- sem efeito e sem linha no log. O usuario percorreu o Insert de 1
  a 6 em seis segundos e apertou Page Down em seguida; bastou a tecla cair um
  frame antes do modo 6 assentar para o toque sumir sem deixar rastro. Agora o
  Page Down sempre cicla e sempre loga, e o modo do Insert decide apenas se o
  resultado e desenhado;
- **o modo 6 logava "aguardando candidato valido" com o depth disponivel.** A
  condicao excluia `ssao_preview_active` e `depth_preview_active`, mas nao
  `rtgi_preview_active`, que e o caminho do modo 6. A mensagem falsa levou a
  leitura errada de que faltava depth no preview do RTGI quando nao faltava;
- os dois defeitos sao da 0.12.0/0.12.1 e mascararam a verificacao do ray
  march: o RTGI vinha rodando e custando, mas as debug views nunca chegaram a
  ser trocadas.

## Pacote 0.13.0 + Photorealism FSR/AA 0.7.1 - 2026-08-28

- **o depth de camera era rejeitado por construcao.** `kMinimumScaledSceneAreaPercent`
  exigia 110% da area da tela, regra escrita para o depth supersampleado do
  ETS2; sem supersampling (`r_scale_x=1`, `r_scale_y=1`) o depth tem
  exatamente 100% e nunca passava. A valvula de escape pedia 400 binds/s e ele
  faz 291/s. Sobrava um shadow map 2048x2048 com 202% da area -- e era ele que
  vencia a selecao;
- por isso RTGI, SSAO e resolve temporal nunca rodaram nas sessoes de teste da
  0.12.1: os tres estavam sem fonte, e nenhum tinha defeito. Cascata de sombra
  deixa de ser vinculada quando nada projeta sombra em vista, e ai
  `Depth sem atividade confirmada por 3 frames` derrubava os tres juntos;
- **forma passa a ser veto, nao bonus.** `is_plausible_scene_shape` aceita a
  proporcao da tela ou a tela multiplicada por fator inteiro em cada eixo, que
  e como o Prism3D faz supersampling. O 1920x2160 do ETS2 supersampleado tem
  proporcao 8:9, longe de 16:9, mas e 1x por 2x: legitimo. O 2048x2048 nao e
  nem uma coisa nem outra;
- **tamanho nativo passa a ser condicao suficiente**, via
  `kMinimumSceneAreaPercent = 95`, ao lado das duas regras que ja existiam. Os
  95% dao folga sem abrir a porta para meia resolucao;
- uma assercao do teste desde a 0.6.x afirmava que um alvo do tamanho exato da
  tela era interface e nunca cena. Era essa rigidez que excluia o depth real.
  A elegibilidade nao precisa dessa separacao porque o score ja a faz: quando o
  mundo supersampleado existe, ele vence. A assercao foi trocada pela
  invariante correta;
- `depth_candidate_rejection` passa a dizer no log **por que** cada candidato
  caiu (`forma-incompativel`, `bindings-insuficientes`, `multisample`,
  `menor-que-metade-da-tela`, `area-e-atividade-insuficientes`). O bug
  sobreviveu versoes porque o log mostrava o score, nunca o motivo. Um teste
  garante que o diagnostico e `is_scene_candidate` nunca divergem;
- nenhum shader foi tocado: `tools/shader_check.sh` da diff zero nos seis.

## Pacote 0.12.2 + Photorealism FSR/AA 0.7.1 - 2026-08-28

- **o plugin deixa de desligar o AA/TAA nativo do jogo.** Ate agora ele forcava
  `r_aa=0`, `r_taa_tuning=0`, `r_taa_luma_sharpen=0.0` e
  `r_taa_modulated_drr_strength=0.0` no `config.cfg` do ETS2/ATS, para assumir
  o AA integralmente. O efeito colateral so apareceu com o RTGI: **com o TAA
  nativo desligado, o Prism3D nao precisa ler o depth num shader e o cria sem
  `D3D11_BIND_SHADER_RESOURCE`**. Sem depth legivel, SSAO, resolve temporal e
  RTGI ficam todos sem fonte;
- foi isso que travou a validacao da 0.12.1: o log mostra o depth de camera
  1920x1080 com `bind_flags=0x00000040 shader_readable=nao`, e o plugin caindo
  num shadow map 2048x2048 -- score 800 vezes menor e proporcao 43,75% fora --
  como unica fonte legivel. Cascata de sombra deixa de ser vinculada quando
  nada projeta sombra em vista, e ai `Depth sem atividade confirmada por 3
  frames` derruba os tres modulos juntos;
- a politica passa a vir da secao `[native_aa.0.12.2]` do
  `photorealism-plugin.cfg`, com `r_aa=6`, `r_taa_luma_sharpen=1.5`,
  `r_taa_tuning=0` e `r_taa_modulated_drr_strength=0.0` como padrao. Da para
  ajustar sem recompilar, e `manage=false` faz o plugin nao tocar no
  `config.cfg` do jogo;
- o dinput8 roda no bootstrap, antes do dxgi, e nao pode usar o `config.cpp`
  da outra DLL. `plugin_config_value` e um leitor minimo do formato
  `[secao]`/`chave=valor`, puro e testado, incluindo secao inexistente, chave
  comentada, contaminacao entre secoes e casamento por prefixo;
- o backup `config.photorealism-native-aa.backup.cfg` continua sendo feito
  antes de qualquer escrita, e a escrita continua atomica.

## Pacote 0.12.1 + Photorealism FSR/AA 0.7.1 - 2026-08-28

- o SSRTGI traca raios pela primeira vez: um raio por pixel, marchado em
  view-space e projetado de volta para a tela a cada passo, com acerto por
  espessura, contribuicao de ceu no miss e alcance util de 0.5 a 15 m;
- `depth_view_space.hlsli` ganha `project_view_position`, inverso exato de
  `reconstruct_view_position`. Fica no mesmo header porque separar as duas
  metades da mesma transformacao e como a duplicacao que a 0.12.0 desfez
  comecou; como nenhum shader aprovado a chama, o bytecode dos quatro
  continuou identico -- verificado com `tools/shader_check.sh`;
- **o resultado ainda nao e composto na imagem.** O buffer RTGI_RAW e
  preenchido e inspecionado pelas debug views; compor e 0.13.2. Nada le o
  buffer, entao a versao continua sem poder piorar a imagem;
- o canal alfa distingue vazio de desconhecido: acerto e ceu valem confianca
  1.0, raio que sai da tela vale 0.0. Screen-space nao tem a informacao nas
  bordas, e marcar isso e o que vai permitir a acumulacao temporal da 0.13.3
  confiar mais em quem sabe;
- `hit_thickness` e `normal_bias` entram no cfg com clamp e teste. Sem o
  primeiro, qualquer coisa atras da geometria contaria como acerto e a luz
  vazaria por tras das paredes;
- `RtgiConstants` cresce de 64 para 80 bytes e ganha `InputNeedsSrgbDecode`,
  que nao existia. Era inofensivo enquanto o shader nao lia cor; agora e
  correcao, porque sem decodificar para linear o bounce sairia claro demais
  nas sombras;
- com o Insert na posicao 6, `Page Down` cicla as debug views:
  `normals -> rays -> hit_distance -> raw_gi -> confidence`. O ciclo e a funcao
  pura `next_rtgi_preview_debug`, testada, em vez de uma cadeia de literais;
- com um raio e sem denoise, `raw_gi` parece ruido. E esperado: a 0.13.3 e a
  0.13.4 sao o que tornam o sinal usavel.

## Pacote 0.12.0 + Photorealism FSR/AA 0.7.1 - 2026-08-28

- fundacao do Screen-Space Ray-Traced Global Illumination (SSRTGI): a tecnica
  aproxima luz indireta de curto/medio alcance reaproveitando depth
  linearizado, normais reconstruidas, scene color e historico temporal, para
  produzir o `color bleeding` que tira da cena o aspecto de CG;
- `shaders/depth_view_space.hlsli` passa a ser a fonte unica da matematica
  depth -> view-space -> normal, que antes estava duplicada em ssao.hlsl,
  temporal.hlsl e depth-preview.hlsl; os parametros que vinham de cbuffer
  viraram argumentos explicitos e as amostras de depth chegam prontas, de modo
  que o header faz matematica e cada shader faz o proprio I/O;
- a igualdade foi provada em bytecode, nao no olho: `tools/shader_check.sh`
  compila com o mesmo `d3dcompiler_47.dll` que o plugin resolve em tempo de
  execucao e com os mesmos flags. `photorealism.hlsl`, `ssao.hlsl` e
  `temporal.hlsl` ficaram byte-identicos -- inclusive as 1716 instrucoes do
  SSAO aprovado;
- `depth-preview.hlsl` mudou de proposito, e apenas nisso: ganhou a guarda do
  `rsqrt` e a validade de normal que o SSAO ja tinha (+1 max, +2 lt, +2 movc no
  histograma de opcodes). Onde o produto vetorial degenerava e o `normalize`
  produzia NaN, o modo 4 do Insert agora mostra preto;
- nova secao `[module.rtgi.0.12.0]` e `src/rtgi_config.hpp`, header-folha puro
  com os sete modos de `rtgi_debug`, a derivacao de resolucao e o clamp dos
  parametros; um cfg editado a mao nao consegue mais descrever um dispatch
  invalido;
- recursos em meia resolucao `R16G16B16A16_FLOAT` (960x540 em Full-HD), com
  `RGB` = luz indireta e `A` = confianca, e o passe `PSRtgi` posicionado antes
  do grading, para que exposure, contraste e LUT alcancem tanto a luz direta
  quanto a indireta;
- modo Insert 6 `rtgi-normals`, que desenha a reconstrucao pelo caminho novo e
  deve ficar identico ao modo 4;
- `Page Up` liga e desliga o RTGI em tempo real, para comparacao A/B sem sair
  do jogo; `End` restaura o que o arquivo de configuracao diz;
- a validacao proibia os literais `VK_F12`, `VK_PRIOR` e `PageUp` em todo o
  `src/`. A proibicao de `VK_F12` era redundante: o que mantem o F12 como tecla
  de screenshot do Steam e o `HookScreenshots(true)`, que a validacao ja
  verifica, e nao a ausencia do literal. Ela foi removida. Restam duas regras
  que dizem o que de fato importa -- o modulo de captura nao pode consultar
  teclado, e `Page Up` so pode existir no passe de pos-processamento, uma
  unica vez;
- **nenhum raio e tracado nesta versao.** O modulo nasce com `enabled=false` e,
  mesmo ligado, devolve luz indireta zerada e nao alimenta a composicao: compor
  zero e neutro, entao a 0.12.0 mede o custo do andaime sem poder piorar a
  imagem. A calibracao visual consolidada permanece intacta.

## Pacote 0.11.4 + Photorealism FSR/AA 0.7.1 - 2026-08-27

- a ABI v6 observa `RSSetState`, `RSSetViewports` e `RSSetScissorRects` e mantem
  um shadow do estado de rasterizacao por contexto; a prova do draw final deixa
  de consultar `RSGetState`/`RSGetViewports`/`RSGetScissorRects` a cada chamada;
- quando um contexto ainda nao tem shadow utilizavel, ele e semeado uma unica
  vez a partir do estado vivo; contexto sem objeto de rasterizer state e o
  default do D3D11, ou seja `ScissorEnable=FALSE`, e o teste de scissor e
  pulado -- a regra de validacao permanece a mesma da 0.7.0;
- draws rejeitados passam a ser agregados por assinatura estrutural, com
  `hits`, primeiro e ultimo frame e amostras da primeira ocorrencia; offsets de
  indice e o ponteiro do depth-stencil view ficam fora da chave de agrupamento
  para nao fragmentar milhares de draws equivalentes em assinaturas distintas;
- `replacement=0` e `dispatch=0` permanecem: nenhuma substituicao de SRV,
  nenhum dispatch de FSR e nenhuma alteracao de viewport, scissor, rasterizer
  state, shader ou render target acontece nesta versao;
- esta entrega existe apenas para responder, com evidencia, se os draws
  rejeitados por `scissor` estao mesmo incorretos;
- resposta obtida em ETS2 1.60/Proton: o passe final e fullscreen mas esta
  dividido em quatro draws scissorados de 960x540 que ladrilham 1920x1080. A
  leitura do estado de rasterizacao estava certa; errada era a conclusao de que
  draws assim nao sao a composicao final. Detalhes em
  `references/draw-proof-tiles-0.7.1.md`.

## Pacote 0.11.3 + Photorealism FSR/AA 0.7.0 - 2026-08-26

- adicionada a prova passiva do draw final via `Draw`, `DrawIndexed`,
  `DrawInstanced` e `DrawIndexedInstanced`;
- o modulo valida o estado D3D11 vivo antes do draw: backbuffer, RTVs, depth,
  source no slot 0, viewport, scissor, pixel shader e topologia fullscreen;
- uma assinatura identica precisa aparecer por 24 frames apresentados para ser
  bloqueada; quebra de assinatura, resize ou perda da prova a invalida;
- esta versao permanece deliberadamente sem efeito visual auxiliar:
  `replacement=0`, `dispatch=0`, EASU/Temporal/RCAS nao executam;
- o log agrega a cada dez segundos as provas validas e os gates rejeitados,
  preparando a ativacao automatica de Temporal + RCAS na proxima etapa.

## Pacote 0.11.2 + Photorealism FSR/AA 0.6.1 - 2026-08-25

- corrigida a substituicao insegura de scene-SRV que podia repetir a imagem em
  quadrantes no menu, garagem ou outras composicoes internas;
- FSR/AA auxiliar passou para fail-closed: binds de SRV continuam observados,
  mas nenhum SRV e trocado ate uma etapa posterior validar o draw final;
- o pipeline visual principal (`dxgi.dll`), incluindo iluminacao, SSAO e
  temporal ja consolidados, continua ativo;
- esta e uma correcao de seguranca: EASU, RCAS e AA temporal auxiliares ficam
  em pass-through ate a integracao de prova por draw.

## Pacote 0.11.1 + Photorealism FSR/AA 0.6.0 - 2026-08-24

- removida do ZIP a pasta `references`, reservada a documentacao interna de
  desenvolvimento;
- binarios core 0.11.0 e FSR/AA 0.6.0 permanecem sem alteracao funcional.

## Core 0.11.0 + Photorealism FSR/AA 0.6.0 - 2026-08-24

- adicionados AA espacial edge-aware e resolve temporal pre-interface com
  historico ping-pong, clamp de vizinhanca, rejeicao de cor e correspondencia
  local 3x3; nao usa jitter/motion vectors nem alega superioridade sem A/B;
- RCAS conservador sucede o temporal em resolucao nativa; EASU continua
  restrito a fonte menor com escala e proporcao seguras;
- selecao automatica aceita a cena R11 nativa/supersampled comprovada por
  composicao direta; resolucao nova exige correlacao com depth;
- bootstrap restrito a ETS2/ATS cria backup unico do `config.cfg` e zera
  atomicamente `r_aa`, `r_taa_tuning`, `r_taa_luma_sharpen` e o cvar DRR se
  presente, com log detected/applied e sem reativacao do nativo;
- TemporalAA, EASU e RCAS possuem telemetria GPU separada, sem I/O no hot path;
- preservados hashes de CFG/quatro shaders visuais, F12 nativo deduplicado e
  Home/End/Insert;
- Home desativado bloqueia tambem a substituicao pre-UI e invalida o historico
  temporal; assim a captura Steam desativada permanece realmente vanilla;
- adicionados testes de transformacao do CFG e selecao R11
  nativa/supersampled, inclusive resolucao nova correlacionada ao depth.

## Core 0.10.4 + Photorealism FSR 0.5.0 - 2026-08-24

- substituida a fila de callbacks Steam por um token unico de captura em voo;
- callbacks repetidos do mesmo toque sao coalescidos durante 750 ms e a
  entrada virtual de call-result nao gera uma segunda solicitacao;
- adicionada telemetria `accepted`, `coalesced`, `write_handle`, `writes` e
  `result` para verificar a cardinalidade real sob Proton;
- mantidos F12 nativo, `HookScreenshots(true)`, readback assincrono e
  `WriteScreenshot` no render thread; nenhuma tecla alternativa foi criada;
- `TriggerScreenshot` permanece restrito a falha comprovada sem handle;
- FSR 0.5.0, configuracao, shaders e resultado visual foram preservados;
- AA/TAA nativos do ETS2/ATS nao foram alterados: o resolve temporal atual nao
  possui jitter, motion vectors ou reprojecao e nao e anunciado como TAA
  substituto.

## Core 0.10.3 + Photorealism FSR 0.5.0 - 2026-08-24

- integrada a implementacao oficial AMD FidelityFX FSR 1 v1.0.2 sob MIT:
  EASU e RCAS agora executam juntos em compute shaders SM5;
- adicionada ABI v4 retrocompativel para observar/substituir somente SRVs do
  passe de composicao; o hook `PSSetShaderResources` e opcional e sua falha
  nunca derruba o core visual;
- selecao deixou de forcar as familias R16F antigas e agora exige prova
  repetida de consumo scene-SRV com o backbuffer exato em OM, fonte menor,
  escala/proporcao segura e formato suportado;
- formatos UNORM/sRGB e R11G11B10 recebem preferencia; R16F continua
  elegivel apenas quando todos os gates comprovam o uso direto;
- o SRV RCAS substitui somente scene-color antes da composicao posterior de
  GPS, textos, menus e UI em resolucao nativa;
- em 100%, 125%, supersampling, baixa confianca ou falha de recursos, o
  modulo permanece em pass-through automatico e continua observando;
- save/restore do estado CS, lifetime seguro de dispositivo/resize e oito
  slots de queries sem flush medem EASU e RCAS separadamente;
- o core usa `ISteamScreenshots` v003: registra `ScreenshotRequested_t`,
  assume F12 sem interceptar tecla, agenda readback assincrono, converte RGB
  em worker de dois slots e chama exatamente um `WriteScreenshot` no render
  thread;
- falha Steam/API/formato/readback mantem ou devolve a captura ao overlay;
  `TriggerScreenshot` recupera somente uma solicitacao sem handle;
- worker de screenshot possui stop event, join e fechamento de handles em
  resize/troca de dispositivo; integracao ausente recebe retry limitado;
- preservados os hashes da configuracao e dos quatro shaders visuais
  consolidados.

## Core 0.10.2 + Photorealism FSR 0.3.0 - 2026-08-24

- adicionada espera limitada de tres segundos e estabilizacao do modulo
  `gameoverlayrenderer64.dll` antes da instalacao do hook, com fallback
  explicito fora do Steam;
- adicionada cobertura de `IDXGISwapChain1::Present1` no slot 22, mantendo
  `Present` e `ResizeBuffers`;
- criado dispatcher reentrante compartilhado por `Present`/`Present1`, para o
  passe executar exatamente uma vez mesmo quando uma apresentacao chama a
  outra na mesma cadeia;
- adicionada auditoria de endereco, modulo proprietario, downstream e estado
  dos entries no install, primeira chamada e 0,5/2/5 segundos depois;
- o core nao intercepta F12 e nao usa `HookScreenshots`/`WriteScreenshot`; a
  compatibilidade da captura nativa permanece criterio de teste real;
- adicionada ABI v3 retrocompativel para contexto automatico de selecao;
- selecionadas defensivamente as familias R16F observadas no ETS2
  (`1920x1352`) e ATS (`2400x1352`), aceitando escala assimetrica;
- selecao exige RT+SRV, sample/mip/array unitarios, familia conhecida,
  atividade recente e doze confirmacoes; correlacao com depth aumenta a
  confianca quando disponivel e recursos concorrentes usam bindings e ordem
  como desempate;
- resolucoes novas podem travar somente quando correlacionadas ao depth;
  familia conhecida continua podendo estabilizar antes dele aparecer;
- confirmacoes agora exigem identidade, dimensoes/formato da fonte e dimensao
  de saida iguais, protegendo contra reuso do endereco de um recurso destruido;
- candidato seguro produz `FSR automatico pronto`; perda retorna ao
  pass-through e a procura continua automaticamente;
- nao existe tecla ou preview FSR. EASU/RCAS continuam ausentes e o modulo nao
  modifica frames;
- preservados byte a byte CFG e shaders consolidados da 0.10.1.

## Photorealism FSR 0.2.0 sobre o nucleo 0.10.1 - 2026-08-22

- estendida a API para ABI v2 sem remover a tabela ABI v1 consolidada;
- adicionados eventos minimos de color RTV para `OMSetRenderTargets` e
  `OMSetRenderTargetsAndUnorderedAccessViews`, excluindo chamadas do proprio
  passe Photorealism;
- criado cache fixo de 4096 RTVs e catalogo de 256 texturas, sem alocacao ou
  escrita de log no hot path de binding;
- `GetResource` e `QueryInterface` ocorrem somente no cache miss de uma view;
- cada recurso registra dimensoes, formato de textura/view, sample count,
  bind/misc flags, mip/array, views, slots, bindings, frequencia e ordem;
- backbuffers sao reconhecidos por identidade COM observada; cena,
  espelhos/reflexos e interface recebem apenas rotulos heurísticos de
  candidatos, sem afirmar semantica proprietaria do Prism3D;
- `Present` agora somente captura um snapshot limitado a cada 30 segundos;
  ordenacao e I/O foram movidos para um unico worker com fila fixa de dois
  slots, sem criacao ilimitada de threads ou uso de recursos COM no worker;
- saturacao da fila e contabilizada por `async_job_drops` e
  `report_queue_drops`, e o encerramento aguarda produtores em curso e drena
  trabalhos pendentes antes de destruir os handles;
- os relatorios permanecem limitados aos 32 recursos mais ativos;
- catalogo e caches sao limpos em resize, mudanca da assinatura do backbuffer
  e troca de dispositivo, sem manter referencias COM aos recursos observados;
- corrigido o rotulo do feature level `0xC100` para `12_1` e adicionados os
  demais niveis D3D relevantes;
- adicionados testes determinísticos para classificacao de apresentacao,
  cena, reflexo, interface e recurso inconclusivo;
- EASU e RCAS permanecem ausentes; nenhum frame e modificado e os hashes dos
  shaders/configuracao consolidados permanecem inalterados.

## Photorealism FSR 0.1.0 sobre o nucleo 0.10.1 - 2026-08-22

- criada `photorealism-fsr.dll` como modulo auxiliar Windows x64, sem exports
  de proxy e carregada explicitamente pelo nucleo `dxgi.dll`;
- definida API C/ABI v1 com tamanho, versao, inicializacao por
  `ID3D11Device` e encerramento coerente antes da troca do dispositivo;
- o modulo recebe somente o dispositivo real do jogo, nunca o dispositivo de
  prova usado para instalar as vtables dos hooks;
- criado `photorealism-fsr.log` separado com feature level, adaptador, memoria,
  capacidades D3D11 e suporte de formatos relevantes para EASU/RCAS;
- ausencia, ABI incompativel ou falha da DLL e tratada como nao fatal e mantem
  o nucleo 0.10.1 ativo;
- EASU e RCAS ainda nao foram implementados; nao existem novos shaders,
  recursos por frame ou alteracoes visuais nesta versao;
- build, validacao e pacote agora incluem a terceira DLL e identificam
  separadamente as versoes do nucleo e do modulo FSR.

## 0.10.1 - 2026-08-22

- corrigido o encerramento definitivo da descoberta quando a primeira janela
  de 30 segundos terminava no menu ou durante o carregamento;
- ciclos sem candidato agora se renovam automaticamente ate a entrada no mundo
  3D, eliminando a necessidade de pressionar `End` para ativar SSAO e temporal;
- adicionada consolidacao antecipada segura depois de tres segundos de
  observacao, usando escala interna ou atividade sustentada de pelo menos 400
  bindings por segundo para distinguir a cena do depth de interface;
- mantido o requisito minimo de 1000 bindings, formato sem MSAA e area
  compativel antes de qualquer candidato poder ser selecionado;
- adicionados testes de regressao que rejeitam o depth `1920x1080` de interface
  registrado no ETS2 e aceitam os depths internos do ETS2 e ATS, inclusive uma
  cena em resolucao nativa identificada pela taxa de uso;
- `Home`, `End` e `Insert` agora usam deteccao de borda confiavel sob Proton;
  `End` permanece disponivel apenas para recarga e diagnostico opcional;
- preservados byte a byte os shaders visual, SSAO e temporal, bem como toda a
  calibracao aprovada na 0.10.0.

## 0.10.0 - 2026-08-21

- iniciada a etapa final de integracao temporal com um resolve conservador
  aplicado depois da pilha visual e do SSAO 0.9.1;
- adicionados historicos independentes de cor e profundidade, recriados para
  cada backbuffer e geracao valida do depth;
- o historico de cor e limitado ao minimo e maximo da vizinhanca 3x3 do frame
  atual, reduzindo rastros de informacao que ja nao existe na cena;
- adicionada rejeicao por diferenca relativa de profundidade para descartar
  desoclusoes, troca de geometria e movimento de camera;
- adicionada rejeicao por diferenca de cor para preservar farois, flares,
  chuva, reflexos e objetos em movimento;
- calibrado o primeiro perfil com `history_weight=0.65`,
  `depth_rejection=0.02` e `color_rejection=0.08`;
- o historico e invalidado em `Home`, `End`, `Insert`, resize, troca de depth,
  recompilacao de shader ou suspensao do passe espacial;
- o modulo `[module.temporal.0.10.0]` pode ser desativado isoladamente,
  retornando integralmente a pilha visual/SSAO consolidada na 0.9.1;
- mantidos byte a byte os shaders visual e SSAO aprovados; o novo efeito fica
  em `temporal.hlsl` e nao injeta jitter nem substitui o TAA nativo do jogo;
- o pacote passa a identificar explicitamente compatibilidade com ETS2 e ATS
  1.60 sob Proton/DXVK.

## 0.9.1 - 2026-08-21

- corrigida a selecao que favorecia um depth `1920x1080` de interface sobre o
  depth interno `1920x2160` do ETS2 mesmo quando este possuia muito mais
  bindings;
- o ranking agora prioriza atividade real, aceita escala interna assimetrica,
  preserva um bonus moderado de proporcao e penaliza recursos quadrados de
  sombras;
- ampliado o limite de contribuicao de bindings de `10000` para `100000`,
  evitando empates artificiais entre recursos com frequencias muito
  diferentes;
- adicionada confianca minima de `1000` bindings para impedir que depths de
  menu sejam consolidados antes da entrada no mundo 3D;
- cada recurso observado recebe uma referencia COM temporaria durante os 30
  segundos de descoberta; somente o vencedor final permanece retido e todas
  as demais referencias sao liberadas imediatamente;
- corrigido o caso do ATS no qual dois recursos de mesma prioridade estatica
  faziam o vencedor por bindings divergir do unico recurso retido;
- adicionados testes de regressao com as medidas reais dos logs do ETS2 e com
  o perfil interno esperado do ATS em escala de 125%;
- preservados byte a byte os shaders visual e SSAO, alem de todos os valores
  da configuracao 0.9.0 e das calibracoes aprovadas.

## 0.9.0 - 2026-08-21

- adicionada a camada independente `[module.ssao_interior.0.9.0]`, mantendo
  integralmente os parametros exteriores aprovados nas versoes 0.7.0 e 0.8.0;
- criada transicao espacial continua do perfil de cabine/interior para o
  perfil exterior entre `near_start=2.0` e `near_end=8.0` metros;
- calibrado o perfil proximo para raio `0.45`, intensidade `0.20`, bias `0.05`
  e rejeicao de bordas `1.75`, preservando detalhes de painel, colunas,
  retrovisores, degraus e outras geometrias proximas;
- mantidas as 16 amostras, a normalizacao e a protecao de altas luzes da 0.8.0;
- desativar somente este novo modulo retorna cada pixel ao perfil exterior,
  sem remover o refinamento SSAO anterior;
- TAA e integracao temporal foram formalmente movidos para a etapa final por
  decisao de projeto; esta versao nao altera esses recursos.

## 0.8.0 - 2026-08-21

- consolidadas a correcao de ciclo de vida 0.7.2 e a aprovacao visual do SSAO
  experimental;
- criada a camada cumulativa `[module.ssao_refinement.0.8.0]`, sem sobrescrever
  os parametros do modulo SSAO 0.7.0;
- ampliada a amostragem de oito para 16 pontos quando o refinamento esta ativo;
- completados dois aneis simetricos de oito direcoes para reduzir padroes em
  cruz, granulado direcional e contatos irregulares;
- normalizada a contribuicao das 16 amostras para preservar a intensidade
  media aprovada, evitando simplesmente escurecer a imagem;
- adicionada protecao de altas luzes em espaco linear para o SSAO atuar menos
  sobre farois, flares, reflexos molhados e outras fontes luminosas;
- mantida uma fracao configuravel de oclusao nas superficies claras para nao
  achatar volumes e encontros geometricos;
- preservados o shader visual, a coloracao, a exposicao e todas as camadas
  cumulativas anteriores;
- mantidas as protecoes de ceu, distancia, silhuetas e depth obsoleto.

## 0.7.2 - 2026-08-21

- consolidada a correcao da mancha causada por depth obsoleto e a aprovacao do
  SSAO experimental da 0.7.1;
- o log do teste confirmou uma invalidacao e redescoberta automatica sem uso
  da tecla `End`;
- identificadas 94 ausencias isoladas de binding e 93 retomadas imediatas no
  mesmo teste, sem falha de dispositivo ou de shader;
- adicionado hook de `ClearDepthStencilView` para reconhecer quando o ETS2
  atualiza o candidato sem repetir `OMSetRenderTargets`;
- adicionada tolerancia de dois frames para reutilizacao segura do ultimo depth
  atualizado, evitando suspensoes isoladas e microvariacao do SSAO;
- a partir do terceiro frame consecutivo sem atividade, o SSAO e suspenso e o
  passe visual photorealista permanece ativo;
- mantida a invalidacao definitiva em 30 frames e a redescoberta automatica
  introduzida na 0.7.1;
- preservados byte a byte os shaders visual e SSAO, assim como toda a
  calibracao cumulativa aprovada.

## 0.7.1 - 2026-08-21

- corrigido o uso de depth buffer obsoleto depois de transicoes entre menu e
  mundo 3D que nao acionam `ResizeBuffers`;
- mantido um monitor leve de atividade depois da descoberta, sem reabrir o
  catalogo completo enquanto o candidato permanece valido;
- excluidas do monitor as chamadas `OMSetRenderTargets*` feitas pelo proprio
  passe do plugin, evitando falsos sinais de atividade;
- o SSAO agora e aplicado somente quando o depth selecionado foi usado pela
  cena desde o frame anterior;
- ao primeiro frame sem atividade, o SSAO e suspenso e o tratamento visual
  photorealista aprovado continua sem manchas ou imagens fantasma;
- depois de 30 frames consecutivos sem atividade, o candidato e liberado e a
  descoberta reinicia automaticamente, sem exigir a tecla `End`;
- adicionada verificacao concorrente da geracao e do contador de bindings para
  impedir invalidacao caso o depth volte a ser usado durante a transicao;
- preservados sem alteracao o shader visual, o perfil cumulativo, o shader
  SSAO e todos os valores da calibracao 0.7.0.

## 0.7.0 - 2026-08-21

- consolidada a validacao estrutural da 0.6.4 a partir das imagens de distancia
  linear e normais reconstruidas;
- adicionado `ssao.hlsl` como passe separado, sem modificar o shader visual
  aprovado;
- criada textura intermediaria sRGB para compor cumulativamente tratamento de
  cor e oclusao ambiente;
- implementado SSAO experimental com oito amostras, reconstrucao de posicao e
  normal em espaco de camera;
- adicionadas protecoes contra ceu, profundidade invalida, saltos de distancia
  e contaminacao entre silhuetas;
- criado fade entre `30` e `70` metros para concentrar o efeito na geometria
  proxima;
- criada secao `[module.ssao.0.7.0]` com raio, intensidade, bias, fade e
  rejeicao de bordas independentes das calibracoes anteriores;
- adicionada mascara de visibilidade SSAO ao final do ciclo da mesma tecla
  `Insert`;
- ampliada a preservacao do estado D3D11 para os dois recursos e samplers usados
  pelo novo passe;
- mantido fallback automatico para o visual aprovado enquanto o depth nao foi
  identificado ou quando qualquer recurso SSAO estiver indisponivel.

## 0.6.4 - 2026-08-21

- consolidada a orientacao reversed-Z e mantida a formula de distancia da
  0.6.3;
- reduzido `preview_distance` de `200.0` para `50.0` para aumentar a separacao
  visual da geometria proxima;
- adicionada reconstrucao aproximada de posicoes em espaco de camera usando
  profundidade linear, proporcao do backbuffer e FOV vertical configuravel;
- adicionada visualizacao RGB de normais reconstruidas a partir dos pixels
  vizinhos da textura depth;
- usada a derivada de menor salto em cada eixo para reduzir contaminacao nas
  bordas entre objetos;
- criada secao independente `[depth.0.6.4]` com `near_plane=0.1`,
  `preview_distance=50.0` e `vertical_fov=60.0`;
- ampliado o ciclo da mesma tecla `Insert` para incluir
  `reconstructed-normals` antes de retornar ao modo normal;
- mantidos sem alteracao o shader visual e todas as camadas cumulativas.

## 0.6.3 - 2026-08-21

- as imagens da 0.6.2 confirmaram alinhamento entre a textura `1920x2160` e a
  camera principal, sem padroes de mapa de sombra;
- confirmado que o ETS2 1.60 testado usa profundidade reversed-Z;
- removida do ciclo atual a visualizacao forward-Z, que serviu apenas para
  determinar a orientacao;
- adicionado modo reversed-Z realcado por escala logaritmica;
- adicionada linearizacao de distancia para projecao reversed-Z com plano
  distante infinito usando `distancia = near_plane / depth`;
- criada secao independente `[depth.0.6.3]` com `near_plane=0.1` e
  `preview_distance=200.0`, recarregavel por `End`;
- mantidos sem alteracao o shader visual e todas as camadas cumulativas;
- `Insert` continua sendo a unica tecla e percorre normal, raw, reversed-Z
  realcado e distancia linear.

## 0.6.2 - 2026-08-21

- consolidado o recurso `1920x2160`, `D32_FLOAT_S8X24_UINT`, sem MSAA, como
  candidato principal do perfil testado em dimensionamento de 200%;
- o observador mantem uma referencia COM somente ao candidato mais forte e a
  libera em reinicio de descoberta, resize ou troca de dispositivo;
- criada copia auxiliar typeless `R32G8X24_TYPELESS` com SRV
  `R32_FLOAT_X8X24_TYPELESS`, sem alterar bind flags ou formato do recurso do
  jogo;
- a copia acontece com o depth original desassociado do pipeline e todo o
  estado D3D11 e restaurado depois do passe;
- adicionado shader independente `depth-preview.hlsl`, preservando sem
  alteracao o shader visual aprovado;
- `Insert` percorre os modos normal, raw, forward-Z e reversed-Z;
- falhas de selecao, formato, dispositivo, textura ou SRV retornam de forma
  segura ao passe visual normal;
- ampliado o cache diagnostico de views para reduzir substituicoes observadas
  no teste da 0.6.1.

## 0.6.1 - 2026-08-21

- removida a exigencia incorreta de que o depth buffer tenha exatamente as
  dimensoes do backbuffer apresentado;
- depth-stencil views agora sao agrupadas pela textura D3D11 subjacente, sem
  contar varias views do mesmo recurso como candidatos independentes;
- ampliado o catalogo para 256 texturas e adicionado cache separado para 512
  views;
- adicionada substituicao controlada dos recursos menos relevantes quando o
  catalogo fica cheio;
- candidatos agora sao classificados por area, semelhanca de proporcao com o
  backbuffer e frequencia real de bindings;
- o relatorio registra grupos por resolucao e os recursos individuais mais
  relevantes, incluindo `area_scale`, `aspect_error`, formatos, MSAA, bind
  flags, quantidade de views e bindings;
- preservados sem alteracao o shader, a calibracao cumulativa aprovada e a
  telemetria GPU.

## 0.6.0 - 2026-08-20

- adicionados hooks de `OMSetRenderTargets` e
  `OMSetRenderTargetsAndUnorderedAccessViews`;
- criado observador de depth-stencil views do contexto imediato;
- catalogadas resolucao, formato da textura, formato da view, MSAA, bind flags
  e frequencia de uso;
- candidatos em resolucao nativa sao classificados pela quantidade de
  bindings durante uma janela de 30 segundos;
- registrado se cada textura permite `D3D11_BIND_SHADER_RESOURCE`, requisito
  para leitura direta por SSAO ou outros passes espaciais;
- descoberta e reiniciada automaticamente quando a resolucao ou formato do
  backbuffer muda;
- `End` reinicia manualmente a janela para permitir coleta dentro do mundo 3D,
  sem depender do tempo gasto no menu;
- apos a janela de descoberta o observador se desliga para reduzir o custo;
- mantidos shader, perfil cumulativo e telemetria GPU da 0.5.1 sem alteracao.

## 0.5.1 - 2026-08-20

- adicionada temporizacao do passe com queries de timestamp D3D11;
- implementado anel de oito amostras para evitar espera pelo resultado do
  DXVK;
- usada leitura `D3D11_ASYNC_GETDATA_DONOTFLUSH`, sem forcar sincronizacao
  entre CPU e GPU;
- agregado relatorio a cada dez segundos com media, minimo, pico, quantidade
  de amostras e descartes;
- queries sao recriadas automaticamente quando o dispositivo D3D11 muda;
- falha ou indisponibilidade da telemetria nao desativa o passe visual;
- mantidos shader, calibracoes cumulativas e resultado visual da 0.5.0.

## 0.5.0 - 2026-08-20

- adicionado hook de `IDXGISwapChain::ResizeBuffers` junto ao hook de
  `Present`;
- recursos intermediarios dependentes de tamanho agora sao liberados antes do
  redimensionamento e recriados no primeiro frame seguinte;
- adicionada deteccao explicita de troca do objeto swap chain e do dispositivo
  D3D11;
- sincronizado o processador para evitar disputa entre `Present` e
  `ResizeBuffers`;
- adicionados logs com tamanho, formato, flags e resultado de cada
  redimensionamento;
- mantidos shader e pilha cumulativa da 0.4.0 sem qualquer nova calibracao
  visual.

## 0.4.0 - 2026-08-20

- reconstruida a arquitetura em duas DLLs com responsabilidades separadas;
- reduzido `dinput8.dll` a bootstrap e proxy das seis funcoes DirectInput;
- criado `dxgi.dll` como proxy das fabricas DXGI e nucleo grafico do plugin;
- movidos hook de `Present`, shader, configuracao e log para o nucleo DXGI;
- adicionada inicializacao unica, independentemente de qual DLL seja carregada
  primeiro;
- substituido o perfil absoluto por uma pilha cumulativa de calibracoes;
- preservadas como camadas a base 0.1.2, a calibracao visual 0.2.0 e a
  calibracao de chuva/tempo nublado 0.3.0;
- validado automaticamente que a composicao final reproduz exatamente os
  valores efetivos aprovados na 0.3.0;
- mantidos o shader, o custo por pixel e os atalhos `Home`/`End` sem alteracao.

## 0.3.0 - 2026-08-20

- calibrado o passe para chuva e tempo nublado sem criar um perfil separado;
- adicionada mascara de medios tons para concentrar clareza em asfalto,
  fachadas e vegetacao;
- protegidas sombras profundas da cabine e altas luzes do ceu contra nitidez
  excessiva;
- realces comprimidos para preservar nuvens e ceu cinzento;
- brancos elevados com moderacao para recuperar reflexos do asfalto molhado;
- nitidez global reduzida e contraste local elevado de forma seletiva;
- preservado o custo de cinco amostras de textura por pixel da versao 0.2.0.

## 0.2.0 - 2026-08-20

- criada a primeira calibracao visual perceptivel do plugin;
- temperatura ajustada para 6400 K, mantendo branco praticamente neutro;
- contraste global elevado para 1.06 e exposicao ajustada para -0.04;
- realces comprimidos para preservar ceu, nuvens e superficies claras;
- sombras medias abertas com pretos mais firmes;
- saturacao controlada e vibrance positiva discreta;
- microcontraste e nitidez reforcados sem novas amostragens de textura;
- mantidos `Home` para toggle e `End` para recarga em tempo real.

## 0.1.2 - 2026-08-20

- alterado o atalho de ativacao/desativacao de `F10` para `Home`;
- alterado o atalho de recarga de `F11` para `End`;
- eliminado o conflito com o comando nativo de screenshot do ETS2;
- mantida a calibracao visual da 0.1.1 para comparacao A/B controlada.

## 0.1.1 - 2026-08-20

- adicionado suporte a `DXGI_FORMAT_B8G8R8A8_UNORM` e
  `DXGI_FORMAT_R8G8B8A8_UNORM`;
- adicionadas views sRGB aceleradas pela GPU para backbuffers `UNORM`;
- adicionadas decodificacao e codificacao sRGB manuais como fallback de
  compatibilidade;
- mantido o caminho automatico de sRGB para backbuffers `UNORM_SRGB`;
- adicionado registro explicito do primeiro frame processado;
- corrigido o caso em que o plugin carregava, mas ignorava o backbuffer de
  formato 87 usado pelo ETS2 1.60 no Proton/DXVK.

## 0.1.0 - 2026-08-20

- criada a arquitetura Windows x64 para ETS2 via Proton/DXVK;
- implementado proxy independente de `dinput8.dll`;
- implementado hook de `IDXGISwapChain::Present` no Direct3D 11;
- criado o primeiro passe de color grading, tons, contraste local e nitidez;
- adicionados configuracao externa, recarga por F11, toggle por F10 e log;
- documentados instalacao segura, limitacoes e roadmap.
