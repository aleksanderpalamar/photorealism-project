# Photorealism FSR 0.1.0 - Fundacao

## Escopo

A versao 0.1.0 introduz apenas a fronteira modular necessaria para desenvolver
FSR 1 sem acoplar a implementacao ao proxy `dxgi.dll`. Ela nao contem EASU,
RCAS, sharpening ou qualquer outro passe visual.

## Arquitetura

`dinput8.dll` continua sendo somente o bootstrap. `dxgi.dll` continua sendo o
unico proxy grafico e carrega explicitamente `photorealism-fsr.dll` quando o
pipeline identifica o dispositivo D3D11 real do jogo. O dispositivo de prova
criado para obter as vtables dos hooks nao e entregue ao modulo.

A funcao exportada `PhotorealismFsrGetApi` negocia a ABI v1. A estrutura
retornada contem tamanho, versao de ABI, versao do modulo e as operacoes
`initialize_device` e `shutdown_device`. A checagem de tamanho permite ampliar
a estrutura no futuro sem interpretar memoria de uma ABI incompativel.

## Ciclo de vida

O modulo recebe e referencia o `ID3D11Device` depois que o pipeline principal
foi inicializado. Antes de o nucleo liberar ou substituir o dispositivo, ele
chama `shutdown_device`. Inicializacoes repetidas com o mesmo ponteiro sao
idempotentes.

O carregamento e dinamico e opcional. Ausencia da DLL, export ausente, ABI
incompativel ou falha de inicializacao apenas produzem uma mensagem no log do
nucleo; o Photorealism Plugin 0.10.1 continua funcionando.

## Diagnostico

`photorealism-plugin/photorealism-fsr.log` registra:

- versao e ABI;
- ponteiro e feature level do dispositivo real;
- descricao, IDs, LUID e memoria informada pelo adaptador DXGI;
- capacidades de threading D3D11;
- disponibilidade do caminho compute relevante;
- suporte de textura, sample, render target e typed UAV para formatos de cor
  candidatos a futuros passes.

Essas informacoes sao observadas pela API D3D11/DXGI publica. Elas nao revelam
nem pressupõem a arquitetura interna proprietaria do Prism3D.

## Impacto

A 0.1.0 nao instala hooks adicionais, nao aloca texturas, nao submete comandos
GPU e nao executa trabalho por frame. Portanto nao ha mudanca visual esperada.
O unico custo ocorre uma vez na inicializacao, durante consultas de capacidade
e escrita do log.

