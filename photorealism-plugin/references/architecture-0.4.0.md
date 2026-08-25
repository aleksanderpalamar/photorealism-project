# Arquitetura 0.4.0

## Objetivo

Separar injecao/entrada do nucleo grafico e impedir que novas calibracoes
apaguem valores historicos ja aprovados.

## Responsabilidades

### dinput8.dll

- encaminha as seis funcoes publicas do DirectInput para a DLL do sistema;
- localiza e carrega a `dxgi.dll` irma fora do bloqueio do carregador;
- nao importa D3D11, DXGI nem o compilador de shaders;
- nao possui configuracao, shader ou hook grafico.

### dxgi.dll

- encaminha `CreateDXGIFactory`, `CreateDXGIFactory1` e
  `CreateDXGIFactory2` para a DLL DXGI do sistema;
- inicializa o nucleo uma unica vez;
- instala o hook de `IDXGISwapChain::Present`;
- controla shader, configuracao cumulativa, atalhos e log.

O bootstrap tambem funciona quando o jogo carrega `dxgi.dll` antes de
`dinput8.dll`: o nucleo pertence somente ao modulo DXGI e usa `INIT_ONCE`.

## Pilha de calibracao

O perfil enviado ao shader e calculado na seguinte ordem:

1. valores absolutos da base 0.1.2;
2. deltas da calibracao visual 0.2.0;
3. deltas da calibracao de chuva/tempo nublado 0.3.0.

O arquivo de configuracao conserva cada camada separadamente. O validador do
projeto interrompe o pacote se a soma divergir do perfil aprovado da 0.3.0.

## Limite desta migracao

A versao 0.4.0 estabelece o caminho de interceptacao DXGI, mas nao fornece
stubs NGX falsos. Integracoes temporais ou de upscaling so serao adicionadas
quando puderem encaminhar corretamente os recursos e os codigos de retorno do
renderizador.
