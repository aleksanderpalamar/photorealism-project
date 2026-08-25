# Estabilidade da atividade do depth 0.7.2

## Evidencia da 0.7.1

O teste confirmou a correcao principal: o plugin invalidou a geracao antiga
apos 30 frames, iniciou uma descoberta automaticamente e nao voltou a produzir
a mancha relatada. O SSAO experimental foi aprovado visualmente.

O mesmo log registrou 94 frames sem um novo `OMSetRenderTargets` e 93 retomadas
imediatas. Isso demonstra que o ETS2 pode manter ou atualizar o mesmo depth sem
refazer seu binding em todos os frames. Tratar cada ausencia isolada como troca
de cena e seguro, mas pode suspender o SSAO por um frame e gerar microvariacao.

## Estrategia da 0.7.2

O monitor considera dois sinais produzidos pelo jogo:

1. o depth selecionado foi vinculado por `OMSetRenderTargets*`;
2. o depth selecionado foi atualizado por `ClearDepthStencilView`.

As chamadas internas do plugin continuam excluidas. Um numero serial unico
avanca para qualquer um dos dois sinais.

A continuidade usa tres estados:

- atividade nova: copia e SSAO normais;
- um ou dois frames sem atividade: reutilizacao curta do depth anterior;
- tres ou mais frames: SSAO suspenso e somente o visual photorealista ativo.

Em 30 frames sem atividade, a verificacao atomica de geracao e serial invalida
o candidato e reinicia a descoberta. Assim, uma variacao normal de submissao
nao causa flicker, enquanto uma transicao real de menu continua convergindo
automaticamente para um recurso atual.

## Escopo visual

Esta etapa nao recalibra imagem. `photorealism.hlsl`, `ssao.hlsl`, a pilha de
cor e os parametros `[module.ssao.0.7.0]` permanecem iguais. O trabalho aumenta
a estabilidade temporal do resultado photorealista aprovado.
