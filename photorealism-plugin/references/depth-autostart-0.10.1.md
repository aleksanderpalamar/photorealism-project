# Inicializacao automatica do depth 0.10.1

## Problema

A descoberta 0.9.1 executava uma unica janela de 30 segundos. Se essa janela
terminasse no menu ou durante o carregamento sem um candidato seguro, a coleta
era desativada. O passe visual continuava funcionando, mas SSAO e temporal
ficavam aguardando ate o usuario pressionar `End`.

## Correcao

Uma janela sem vencedor agora limpa somente seu catalogo temporario e inicia o
proximo ciclo automaticamente. A coleta continua ate o mundo 3D apresentar um
candidato que cumpra todos estes requisitos:

- textura sem MSAA;
- pelo menos metade da area do backbuffer;
- pelo menos 1000 bindings observados;
- area interna igual ou superior a 110% do backbuffer, ou atividade sustentada
  de pelo menos 400 bindings por segundo.

Depois de tres segundos de observacao, um candidato que cumpra os requisitos
pode ser consolidado antecipadamente. Assim o depth interno escalado do ETS2 ou
ATS entra rapidamente, enquanto o depth `1920x1080` de interface registrado no
ETS2 continua rejeitado por sua taxa de uso menor.

## Garantias

- `End` nao e necessario para a inicializacao normal;
- troca de resolucao, dispositivo ou candidato obsoleto continua reiniciando a
  descoberta automaticamente;
- os shaders visual, SSAO e temporal nao foram modificados;
- nenhuma calibracao visual foi alterada na 0.10.1.
