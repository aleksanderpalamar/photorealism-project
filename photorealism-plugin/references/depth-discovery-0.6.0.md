# Descoberta de profundidade 0.6.0

## Objetivo

Identificar o depth buffer principal usado pelo ETS2 antes de implementar
SSAO, nevoa baseada em profundidade ou qualquer efeito espacial. Nenhum
recurso e selecionado apenas pelo nome ou por uma suposicao de formato.

## Observacao

Os hooks de `OMSetRenderTargets` e
`OMSetRenderTargetsAndUnorderedAccessViews` observam somente o contexto D3D11
imediato. Cada depth-stencil view e catalogada uma vez; bindings posteriores
apenas aumentam seu contador.

Sao registrados:

- largura e altura;
- formato da textura e formato da depth-stencil view;
- quantidade de amostras MSAA;
- bind flags originais;
- possibilidade de leitura por shader;
- quantidade de vezes que a view foi vinculada.

Views com as mesmas dimensoes do backbuffer sao candidatas. A mais vinculada e
apresentada como `Depth candidato #1`.

## Custo controlado

A descoberta dura 30 segundos para cada assinatura de backbuffer. Depois o
observador e desativado. Uma mudanca real de resolucao ou formato limpa o
catalogo e inicia outra janela. Como a primeira coleta pode ocorrer no menu,
`End` reinicia manualmente os 30 segundos quando o caminhao ja estiver no mundo
3D.

## Decisao posterior

- `shader_readable=sim`: criar uma SRV compativel e validar linearizacao;
- `shader_readable=nao`: estudar interceptacao da criacao da textura para
  acrescentar uma view segura, sem modificar recursos arbitrarios;
- MSAA maior que 1: planejar resolve/ amostragem especifica antes do SSAO.
