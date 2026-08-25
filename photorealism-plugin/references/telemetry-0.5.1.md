# Telemetria GPU 0.5.1

## Escopo medido

Os timestamps envolvem os comandos GPU adicionados pelo plugin:

1. copia do backbuffer para a textura intermediaria;
2. atualizacao e execucao do passe de tela cheia.

Captura/restauracao de estado e leitura das teclas ocorrem na CPU e nao fazem
parte desse valor.

## Metodo

Cada amostra usa uma query `D3D11_QUERY_TIMESTAMP_DISJOINT` e duas queries
`D3D11_QUERY_TIMESTAMP`. O nucleo mantem oito conjuntos em anel. Resultados
sao consultados posteriormente com `D3D11_ASYNC_GETDATA_DONOTFLUSH`.

Se todos os conjuntos ainda estiverem pendentes, o frame nao e medido. O
plugin nunca espera a GPU apenas para produzir telemetria.

## Relatorio

A cada dez segundos com amostras validas, o log apresenta:

- tempo medio em milissegundos;
- menor tempo observado;
- maior tempo observado;
- numero de amostras validas;
- numero de amostras descartadas ou sem slot livre.

O objetivo e estabelecer uma linha de base antes de SSAO, antialiasing
temporal ou outros passes mais caros.
