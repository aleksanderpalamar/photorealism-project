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

## FSR 0.5.0 - EASU + RCAS automaticos (consolidado)

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

## Core 0.11.0 + FSR/AA 0.6.0 - substituicao automatica (teste atual)

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
