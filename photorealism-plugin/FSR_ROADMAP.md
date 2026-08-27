# Photorealism FSR - Roadmap

Este roadmap pertence ao modulo auxiliar `photorealism-fsr.dll`. A numeracao
FSR e independente da versao do nucleo Photorealism Plugin.

Regra permanente: inicializacao, selecao, execucao futura e fallback sao
automaticos. O FSR nao ganha tecla de ativacao, preview manual ou modo que o
usuario precise religar ao entrar no jogo. Se uma condicao segura nao for
confirmada, o modulo fica em pass-through e continua tentando sozinho.

## FSR 0.1.0 - Fundacao e diagnostico D3D11 (consolidado)

- DLL auxiliar Windows x64 carregada explicitamente por `dxgi.dll`;
- API C/ABI v1 com consulta versionada, inicializacao e encerramento;
- inicializacao somente com o dispositivo D3D11 real do jogo;
- log proprio com feature level, adaptador, memoria e suporte de formatos;
- falha ou ausencia do modulo tratada como nao fatal;
- nenhuma execucao de EASU ou RCAS;
- nenhuma alteracao visual e nenhum custo por frame.

## FSR 0.2.0 - Observador de render targets de cor (consolidado)

- ABI v2 retrocompativel com a inicializacao v1 consolidada;
- eventos minimos de `OMSetRenderTargets` e da variante com UAVs;
- exclusao das chamadas emitidas pelo proprio passe Photorealism;
- cache fixo de 4096 views e catalogo de 256 texturas, sem alocacao por evento;
- `GetResource`/`QueryInterface` somente na primeira aparicao de cada RTV na
  janela, com contadores protegidos por tentativa de lock;
- resolucao, formatos de textura/view, MSAA, bind/misc flags, views, slots,
  frequencia e ordem de atividade;
- evidencia exata de recursos de apresentacao pela identidade do backbuffer;
- rotulos heurísticos `probable-scene`, `probable-mirror-reflection` e
  `probable-interface`, sempre tratados como candidatos nao confirmados;
- snapshot limitado a cada 30 segundos e fila fixa de dois slots;
- ordenacao e escrita dos 32 alvos mais ativos em um unico worker, sem I/O na
  thread de `Present`, com descarte contabilizado se a fila saturar;
- invalidacao em resize, assinatura do backbuffer e troca de dispositivo;
- nenhum recurso COM retido pelo catalogo e nenhuma alteracao do frame;
- EASU e RCAS continuam ausentes.

## FSR 0.3.0 - Selecao automatica da cena 3D (consolidado)

- ABI v3 retrocompativel, negociada automaticamente pelo nucleo 0.10.2;
- contexto por frame com dimensao de apresentacao e depth selecionado;
- familias verificadas `1920x1352` (ETS2) e `2400x1352` (ATS), ambas
  `R16G16B16A16_FLOAT`, sem rejeitar a escala assimetrica do ETS2;
- requisitos de RT+SRV, uma amostra, um mip, uma camada e atividade recente;
- correlacao dimensional com depth usada como evidencia adicional quando a
  escala coincide, sem rejeitar a familia assimetrica conhecida;
- familia conhecida pode travar sem depth; familia/resolucao nova exige
  correlacao exata com o depth antes de travar;
- desempate de recursos da mesma familia por confianca, bindings e ordem;
- trava logica somente depois de doze confirmacoes consecutivas;
- identidade, source/output e formato precisam permanecer iguais; eventual
  reuso do ponteiro COM com outra assinatura reinicia a confirmacao;
- mensagem `FSR source selecionado` com source/output/confianca;
- perda do candidato retorna ao pass-through e reinicia a busca sozinha;
- nenhum recurso COM retido, nenhum preview visual e nenhuma tecla;
- EASU e RCAS continuam ausentes e nenhum frame e modificado pelo FSR.

## Core 0.10.4 - screenshot Steam oficial deduplicado (consolidado)

Este criterio valida a captura oficial do core; nao e um efeito FSR:

1. com `Home` ativo, pressionar uma vez o F12 nativo do Steam e confirmar o
   efeito Photorealism na captura;
2. com `Home` desativado, pressionar uma vez F12 e confirmar imagem vanilla;
3. cada toque deve criar exatamente uma captura e ela deve continuar no
   uploader/gerenciador normal do Steam;
4. nao existe hook de `VK_F12` nem tecla substituta;
5. o core registra `ScreenshotRequested_t` pela API oficial Steamworks,
   assume a captura com `HookScreenshots(true)`, faz readback assincrono do
   backbuffer ja processado e chama um unico `WriteScreenshot` no render
   thread;
6. somente um token pode permanecer em voo e callbacks repetidos dentro de
   750 ms sao coalescidos; o log deve mostrar `accepted`, `coalesced`,
   `write_handle` e `result=ok`;
7. se a API/interface/callback/readback falhar, o core nao assume ou devolve o
   hook ao overlay e usa `TriggerScreenshot` somente para recuperar a
   solicitacao que ainda nao gerou handle;
8. verificar no log deteccao do overlay, `Present1`, auditoria da cadeia e
   ativacao de `ISteamScreenshots v003`.

## FSR 0.5.0 - EASU + RCAS automaticos (implementacao substituida)

- ABI v4 retrocompativel e hook opcional de `PSSetShaderResources`;
- prova repetida de uma relacao scene-SRV -> backbuffer em OM antes de
  modificar qualquer binding;
- selecao dinamica de resolucoes menores, sem restringir ETS2/ATS a duas
  familias R16F antigas que podem representar outro dominio da cena;
- formatos R16F, R11G11B10, RGBA8/BGRA8 UNORM e sRGB, com preferencia por
  formatos mais compativeis com color pos-tone-map;
- gates: fonte menor nos dois eixos, 1,05x-2,00x, diferenca de escala por eixo
  <=2,5%, erro de proporcao <=1,5%, sample/mip/array unitarios e doze
  confirmacoes;
- resolucao dinamica desconhecida exige correlacao dimensional com o depth
  ativo; isso impede promover uma textura de UI apenas por ser composta no
  backbuffer;
- R16F nunca e forcado: exige a mesma prova de composicao e todos os gates;
- EASU oficial GPUOpen v1.0.2 seguido imediatamente por RCAS conservador de
  0,4 stop, ambos em compute SM5;
- substituicao somente do SRV scene-color no passe de composicao; GPS, textos,
  menus e UI posteriores continuam em resolucao nativa;
- save/restore dos estados CS usados, recursos recriados por assinatura e
  telemetria GPU separada EASU/RCAS sem flush;
- nenhum I/O no hot path; logs passam pelo worker diagnostico limitado;
- em 100%, 125%, supersampling, formato/proporcao insegura, baixa confianca ou
  falha de hook/recurso, pass-through automatico e procura continua;
- nenhuma tecla, preview ou ativacao manual.

O ganho de desempenho depende de o Prism3D realmente fornecer scene-color
menor. O modulo nao reduz sozinho o custo dos passes anteriores da engine.

## Core 0.11.0 + FSR/AA 0.6.0 - substituicao automatica (substituida)

- bootstrap restrito aos executaveis ETS2/ATS, backup unico reversivel e
  escrita atomica de `r_aa=0`, `r_taa_tuning=0`,
  `r_taa_luma_sharpen=0.0` e DRR TAA `0.0` quando presente;
- nenhuma reativacao automatica do AA/TAA nativo quando o modulo estiver em
  pass-through; o diagnostico continua sozinho;
- AA espacial edge-aware seguido de historico ping-pong, clamp 3x3, rejeicao
  de cor e busca local 3x3 de correspondencia;
- ausencia declarada de jitter e motion vectors: nao e reprojecao completa e
  nao existe alegacao de qualidade superior sem evidencia A/B;
- aplicacao somente ao scene-color comprovado antes de GPS/textos/menus/UI;
- R11 nativo/supersampled exige prova forte de composicao; resolucao nao
  nativa nova exige correlacao dimensional com depth;
- EASU somente para fonte menor/proporcao segura; RCAS sucede o temporal tanto
  no caminho nativo quanto no caminho EASU;
- telemetria GPU separada TemporalAA/EASU/RCAS, sem `Flush` ou I/O no hot path;
- ativacao inteiramente automatica, sem tecla nova; Home/End/Insert e F12
  permanecem com as funcoes consolidadas.

Esta abordagem usava a observacao de `PSSetShaderResources` como se ela fosse
prova de composicao. O teste em ETS2 revelou imagem em quadrantes; portanto ela
nao e liberada para uso e foi substituida pelo fail-closed 0.6.1.

## FSR 0.6.1 - fail-closed de composicao (consolidado)

- nenhum bind de SRV altera o frame;
- o modulo continua registrando candidatos, mas EASU, Temporal e RCAS ficam em
  pass-through;
- corrigido o flicker/imagem em quadrantes no ETS2;
- o nucleo `dxgi.dll` segue com iluminacao, SSAO e temporal consolidados.

## Core 0.11.3 + FSR/AA 0.7.0 - prova passiva do draw final

- ABI v5 adiciona observacao de `Draw`, `DrawIndexed`, `DrawInstanced` e
  `DrawIndexedInstanced`;
- a sombra dos 128 slots PS e apenas um pre-filtro; a prova consulta o estado
  vivo do contexto D3D11 imediatamente antes do draw;
- exige RTV0 no backbuffer registrado, nenhum RTV adicional ou depth, source
  elegivel no slot 0, viewport/scissor fullscreen, pixel shader e topologia de
  triangulo fullscreen;
- uma assinatura de source, backbuffer, shader, chamada e dimensoes precisa
  ser identica por 24 frames apresentados para ser bloqueada;
- resize, troca de dispositivo, Home ou perda prolongada da assinatura limpam
  a prova;
- `replacement=0` e `dispatch=0`: nao ha alteracao visual nesta etapa;
- os logs agregados a cada dez segundos registram provas e gates rejeitados
  sem I/O por frame.

## Core 0.11.4 + FSR/AA 0.7.1 - revisao passiva dos draws rejeitados (concluida)

- ABI v6 observa `RSSetState`, `RSSetViewports` e `RSSetScissorRects` e mantem
  um shadow do estado de rasterizacao por contexto, em vez de consultar o
  contexto D3D11 a cada draw;
- a regra de validacao nao muda: `ScissorEnable == FALSE` continua dispensando
  o retangulo de scissor, exatamente como na 0.7.0;
- cada draw rejeitado vira uma assinatura estrutural agregada com `hits`,
  primeiro/ultimo frame e amostras, em vez de milhares de linhas repetidas;
- `replacement=0` e `dispatch=0`: esta etapa continua puramente diagnostica;
- objetivo unico: descobrir se os draws rejeitados por `scissor` estao mesmo
  incorretos ou se a leitura do estado de rasterizacao e que estava errada.

Resultado, detalhado em `references/draw-proof-tiles-0.7.1.md`: nenhuma das duas
hipoteses. O passe final do Prism3D e fullscreen mas esta dividido em quatro
draws scissorados de 960x540 que ladrilham 1920x1080, todos com viewport
fullscreen, mesmo pixel shader, scene color R11G11B10_FLOAT no slot 0,
backbuffer exato como RTV e sem depth. A regra de scissor le o estado
corretamente; o que estava errado era concluir que draws assim nao sao a
composicao final.

## Core 0.11.5 + FSR/AA 0.7.2 - composicao por tiles comprovada (planejado)

Prova, ainda diagnostica:

- aceitar scissor que seja sub-retangulo de um viewport fullscreen, e somente
  quando todos os outros criterios da 0.7.0 ja tiverem passado; scissor parcial
  com viewport parcial continua rejeitado, porque e HUD, interface, espelho ou
  reflexo;
- nao assumir quatro tiles nem 960x540: descobrir a particao acumulando os
  retangulos observados dentro do frame e exigir que a uniao cubra a render
  target sem sobreposicao;
- exigir que todos os tiles do frame compartilhem source, RTV, pixel shader,
  topologia e viewport, e que o conjunto se repita identico por 24 frames
  apresentados, como a 0.7.0 ja exige da assinatura unica;
- `direct_composition_hits` saindo de zero e o primeiro sinal de que a regra
  funcionou.

Ativacao, so depois da prova estavel:

- executar Temporal + RCAS uma unica vez por frame, antes do primeiro tile,
  para uma textura processada; EASU continua desativado nesta etapa;
- substituir o SRV em todos os tiles daquele frame e restaurar o original
  depois do ultimo; substituir tile a tile reproduz o artefato de quadrantes
  corrigido na 0.11.2;
- se qualquer tile do frame divergir da assinatura, abandonar a substituicao do
  frame inteiro e voltar ao draw nativo, nunca a meio caminho;
- somente source nativo de mesma resolucao do backbuffer;
- nenhuma tecla adicional e nenhuma alteracao no fluxo de F12 nesta etapa.

Infraestrutura necessaria antes:

- a tabela de assinaturas rejeitadas precisa de mais de 64 entradas, ou de
  particao por motivo: a classe de draws com depth ligado saturou e perdeu
  entre 30 e 107 assinaturas por janela na captura da 0.7.1.

## Consolidacao

- testar ETS2 e ATS em cidade, rodovia, cabine, camera externa, noite, chuva,
  espelhos, menus e transicoes;
- testar resize, alt-tab, tela cheia/janela, troca de dispositivo e shutdown;
- medir GPU, CPU, VRAM, largura de banda e frame pacing;
- documentar separadamente limitacoes de Proton/DXVK e diferencas entre os
  dois jogos.

O FSR 1 e um upscaler espacial. Ele nao usa vetores de movimento e nao deve ser
confundido com FSR 2/3 ou frame generation. O ganho de desempenho so existira
quando a cena for realmente renderizada abaixo da resolucao de saida; aplicar
um filtro ao backbuffer final nao reduz o custo anterior do Prism3D.
