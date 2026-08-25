# Photorealism FSR 0.3.0 - selecao automatica

## Fronteira desta fase

A ABI v3 estende o prefixo v1/v2 com uma unica operacao de selecao. O nucleo
entrega as dimensoes do backbuffer e do depth ja descoberto. O modulo devolve
somente estado e metadados copiados; nenhuma textura COM atravessa a ABI e
nenhum recurso observado e retido.

Nao ha EASU, RCAS, draw, dispatch, copy ou substituicao da fonte visual nesta
fase. O fluxo e pass-through mesmo depois de `FSR automatico pronto`.

## Classificador

Um candidato precisa ser `R16G16B16A16_FLOAT`, sample count 1, mip 1, array 1,
possuir os binds `RENDER_TARGET` e `SHADER_RESOURCE`, nao ser cubemap nem o
backbuffer e ter sido usado nos dois frames mais recentes. As assinaturas
observadas aceitas sao:

- ETS2: `1920x1352`;
- ATS: `2400x1352`.

A proporcao do backbuffer nao e requisito: isso preserva a escala assimetrica
observada no ETS2. Coincidencia dimensional com o depth ativo aumenta a
confianca quando as escalas sao iguais, mas nao rejeita uma das duas familias
conhecidas. Quantidade de bindings, slot zero e ordem resolvem concorrencia
entre texturas da mesma familia.

Uma familia conhecida pode ser travada sem depth. Uma resolucao desconhecida
so e elegivel para trava quando coincide com o depth ativo, permitindo evoluir
sem aceitar arbitrariamente qualquer RT R16F.

O mesmo token de identidade, dimensoes/formato de fonte e dimensoes de saida
deve vencer doze atualizacoes consecutivas. Isso reinicia a confirmacao se um
endereco COM for reutilizado para uma textura com assinatura diferente. A
selecao pronta e perdida somente apos trinta frames sem o recurso, quando o
modulo registra fallback e volta a procurar. Tokens nunca sao dereferenciados.

## Custo e concorrencia

`OMSetRenderTargets` continua sem alocacao ou log. A atualizacao v3 percorre no
maximo 256 registros sob o lock do catalogo e executa somente aritmetica sobre
metadados. Notices sao transferidos para a fila fixa e escritos pelo worker;
nao ha I/O no `Present`.

## Regra operacional

O FSR inicializa e seleciona automaticamente. Nao existe tecla, preview ou
modo manual. Falha de ABI, ausencia de depth, baixa confianca, resize ou troca
de dispositivo preservam a imagem normal e mantem/reiniciam a observacao.
