# Photorealism FSR 0.2.0 - Observador de cor

## Objetivo

Reconstruir somente por evidencia D3D11 quais texturas de cor podem representar
a cena antes da composicao da interface. Esta fase nao seleciona uma textura
para processamento e nao implementa FSR 1.

## Integracao

A ABI v2 preserva o prefixo e a tabela v1. Um host v1 pode continuar
inicializando e encerrando o dispositivo; o nucleo 0.10.1 negocia v2 e passa
eventos minimos das duas operacoes `OMSetRenderTargets`. O `thread_local` que
protege o `Present` impede que os bindings produzidos pelo shader Photorealism
entrem no catalogo.

Um evento contem somente flags, quantidade e o array transitorio de RTVs. O
modulo nao guarda essas views. `Present` fornece assinatura e identidade
transitoria do backbuffer; resize fornece apenas uma razao de reset.
Chamadas da variante UAV que usam
`D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL` sao ignoradas porque nao alteram
os RTVs atuais.

## Hot path

O modulo usa estruturas de tamanho fixo:

- 4096 entradas de cache por ponteiro de RTV;
- 256 recursos `Texture2D` por janela;
- no maximo oito slots por evento;
- tentativa de `SRWLOCK`, descartando e contando o evento em caso de disputa;
- nenhuma alocacao e nenhum log em `OMSetRenderTargets`.

Em cache hit, somente contadores inteiros sao atualizados. Em cache miss,
`GetResource`, `QueryInterface(IID_ID3D11Texture2D/IUnknown)`, `GetDesc` da
textura e da view sao executados uma vez. Todas as referencias obtidas sao
liberadas imediatamente. O catalogo guarda apenas tokens de identidade e
metadados copiados.

Ponteiros COM podem ser reutilizados depois da destruicao de um objeto. A
janela curta, o reset em resize/dispositivo e a capacidade grande do cache
reduzem esse risco, mas o relatorio continua sendo diagnostico e nao deve ser
usado sozinho para selecionar uma textura na etapa 0.3.0.

## Janela e relatorio

Cada assinatura valida inicia uma janela de 30 segundos. Ao final, o modulo
copia no maximo 256 entradas sob lock, inicia uma janela nova e publica o
snapshot em uma fila fixa de dois slots. `Present` nao ordena os dados, nao
abre arquivos e nao escreve no log. Um unico worker persistente ordena e grava
os relatorios. Nao sao criadas threads por janela.

Um slot pode estar em processamento enquanto o outro aguarda. Se ambos
estiverem ocupados, a janela e descartada de forma controlada e os contadores
`async_job_drops` e `report_queue_drops` aparecem no proximo relatorio aceito.
O worker usa somente metadados copiados: nenhuma view, textura ou referencia
COM e entregue a ele. No encerramento do dispositivo, produtores em fase de
snapshot terminam, a fila e drenada e somente entao os handles do worker sao
fechados. Sao informados:

- resolucao e formato do backbuffer;
- frames, eventos, variante UAV e bindings de slots;
- recursos/views, substituicoes, overflow, views nao suportadas e disputa;
- dimensoes, formatos, MSAA, flags, mip/array e quantidade de views;
- bindings, taxa por segundo, slot mask e primeira/ultima ordem/frame.

Os 32 alvos mais ativos sao escritos para limitar I/O.

## Classificacao conservadora

`presentation-evidence` exige igualdade de identidade com um backbuffer
observado. Os demais rotulos combinam area, aspect ratio, parcela de bindings,
slot zero e posicao temporal:

- `probable-scene`;
- `probable-mirror-reflection`;
- `probable-interface`;
- `unclassified`.

Esses nomes sao hipoteses reproduziveis, testadas por funcao pura, e nao
confirmacao do render graph do Prism3D. A confirmacao visual pertence a 0.3.0.

## Impacto visual

Nao ha shader novo, textura de efeito, copy, dispatch ou draw. EASU/RCAS nao
sao executados e o backbuffer nunca e alterado pelo modulo FSR 0.2.0.
