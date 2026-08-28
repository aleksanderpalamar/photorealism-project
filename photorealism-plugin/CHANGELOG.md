# Changelog

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
  preenchido e inspecionado pelas debug views; compor e 0.12.2. Nada le o
  buffer, entao a versao continua sem poder piorar a imagem;
- o canal alfa distingue vazio de desconhecido: acerto e ceu valem confianca
  1.0, raio que sai da tela vale 0.0. Screen-space nao tem a informacao nas
  bordas, e marcar isso e o que vai permitir a acumulacao temporal da 0.12.3
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
- com um raio e sem denoise, `raw_gi` parece ruido. E esperado: a 0.12.3 e a
  0.12.4 sao o que tornam o sinal usavel.

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
