# Core 0.10.4 - captura F12 unica no Steam

O contrato continua sendo o `ISteamScreenshots` v003 oficial: o core nao le
nem intercepta `VK_F12`; `HookScreenshots(true)` transfere ao jogo a resposta
ao `ScreenshotRequested_t` e um `WriteScreenshot` entrega a imagem RGB ao
uploader normal.

A 0.10.3 limitava a fila, mas contabilizava cada callback como uma solicitacao
independente. Sob uma camada de compatibilidade, uma entrega repetida ou uma
entrada indevida pelo overload de call-result podia produzir dois
`WriteScreenshot` para um unico toque. A 0.10.4 troca a fila por um token:

- somente um ciclo pode existir entre callback aceito e `WriteScreenshot`;
- callback recebido enquanto esse ciclo esta em voo e coalescido;
- depois de concluir, uma janela de 750 ms coalesce a repeticao imediata;
- a entrada virtual de call-result nao gera captura;
- callback, readback e conversao continuam sem consulta de teclado;
- `TriggerScreenshot` continua restrito a uma falha comprovada antes de obter
  um handle valido.

O log de uma captura valida deve conter uma unica linha
`Steam screenshot 0.10.4 concluido` com `result=ok`, `write_handle`, total
`writes`, `accepted` e `coalesced`. Dois callbacks podem elevar `coalesced`,
mas nao `writes`.

Evidencia local anterior ao hotfix: o teste ETS2 de 2026-08-24 gerou um unico
arquivo e uma unica entrada `screenshots.vdf` para `20260824135944_1.jpg` em
1920x1080. Como a 0.10.3 nao registrava callback/handle, essa evidencia nao
permitia separar uma percepcao de duas notificacoes de duas chamadas internas.
A telemetria 0.10.4 remove essa ambiguidade.

Teste real obrigatorio no Proton/DXVK:

1. no mundo 3D e com `Home` ativo, pressionar F12 uma vez;
2. confirmar uma unica entrada persistida com os efeitos;
3. repetir com `Home` desativado e confirmar uma unica imagem vanilla;
4. repetir depois de alt-tab/resize;
5. conferir que cada toque acrescenta exatamente um a `writes`.
