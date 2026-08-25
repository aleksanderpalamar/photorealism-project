# Robustez DXGI 0.5.0

## Problema tratado

`IDXGISwapChain::ResizeBuffers` exige que referencias aos buffers antigos
tenham sido liberadas. Embora o passe nao conserve o backbuffer entre frames,
seus recursos intermediarios dependem de largura, altura e formato. A versao
0.5.0 passa a tratar essa transicao explicitamente.

## Sequencia

1. o hook recebe a solicitacao de `ResizeBuffers`;
2. textura e SRV intermediarias sao liberadas;
3. a chamada original e encaminhada ao DXGI/DXVK;
4. o resultado e registrado no log;
5. no primeiro `Present` valido, os recursos sao recriados com a nova
   descricao do backbuffer.

O pipeline, os shaders e a pilha de calibracao permanecem carregados quando o
dispositivo D3D11 continua sendo o mesmo. Se o dispositivo mudar, o pipeline
completo e reinicializado.

## Sincronizacao

Um `SRWLOCK` exclusivo protege o estado do processador durante `Present` e
`ResizeBuffers`. O swap chain e rastreado por identidade sem reter uma
referencia COM adicional. Enquanto a chamada original de `ResizeBuffers` esta
em andamento, novos `Present` sao encaminhados sem executar o passe grafico.

## Teste recomendado

- iniciar na resolucao habitual e confirmar o efeito com `Home`;
- usar Alt+Tab e retornar ao jogo;
- alternar entre janela e tela cheia, se essa configuracao for usada;
- mudar a resolucao e retornar a original;
- confirmar no log `ResizeBuffers solicitado`, `ResizeBuffers concluido` e
  uma nova mensagem `Recursos de frame criados`.
