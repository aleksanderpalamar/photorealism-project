# Resolve temporal 0.10.0

## Objetivo

Reduzir cintilacao fina em vegetacao, cercas, linhas e detalhes distantes sem
substituir o TAA nativo do ETS2 ou ATS e sem reutilizar informacao de outra
cena.

## Ordem cumulativa

1. copia nao destrutiva do backbuffer;
2. shader visual aprovado;
3. SSAO interior/exterior aprovado;
4. resolve temporal 0.10.0;
5. copia do resultado e do depth atuais para os historicos.

Os shaders visual e SSAO permanecem inalterados. Desativar apenas
`[module.temporal.0.10.0]` devolve exatamente a composicao da 0.9.1.

## Protecoes

- clamp RGB pelo minimo e maximo da vizinhanca 3x3 atual;
- rejeicao por diferenca relativa entre depth atual e historico;
- rejeicao por distancia de cor depois do clamp;
- ceu aceito somente quando os dois depths representam ceu;
- historico invalido no primeiro frame e depois de qualquer transicao;
- invalidacao em `Home`, `End`, `Insert`, resize, troca de depth e recompilacao.

Sem vetores de movimento, o passe nao tenta reprojetar pixels. Essa escolha
torna a primeira calibracao mais conservadora: o historico ajuda onde a imagem
permanece coerente e perde peso rapidamente onde existe movimento ou
desoclusao.
