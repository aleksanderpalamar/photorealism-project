# Selecao de depth 0.9.1

## Evidencia de reproducao

No ETS2, uma janela de descoberta registrou:

| Recurso | Bindings | Resultado antigo |
| --- | ---: | --- |
| `1920x1080` | 7.499 | selecionado |
| `1920x2160` | 32.818 | rejeitado pelo peso do aspect ratio |
| `4096x4096` | 7.236 | candidato de sombra |

O recurso `1920x1080` deixou de ser atualizado repetidamente, causando 1.168
suspensoes e 1.163 retomadas do SSAO. O fallback evitou manchas, mas o efeito
alternava durante o jogo.

No ATS, dois recursos `1920x1080` de menu receberam 140 e 116 bindings. O
vencedor final nao coincidia necessariamente com a unica referencia COM
mantida pelo algoritmo anterior, portanto nenhum depth era consolidado.

## Correcao

O ranking 0.9.1 combina:

- area do recurso;
- frequencia de bindings ate o limite de 100.000;
- bonus moderado para dimensoes iguais ou maiores que o backbuffer;
- bonus por compartilhar um eixo com o backbuffer, cobrindo o `1920x2160`;
- bonus moderado para aspect ratio proximo;
- penalidade forte para recursos quadrados quando a tela nao e quadrada.

Um candidato tambem precisa ter uma unica amostra, pelo menos metade da area
do backbuffer e no minimo 1.000 bindings durante a janela de 30 segundos.

Todos os recursos catalogados recebem uma referencia COM temporaria. Depois da
classificacao, o vencedor recebe sua propria referencia e as referencias
temporarias sao liberadas. Isso permite selecionar pelo resultado final sem
reter texturas perdedoras durante o restante da sessao.

## Regressao

`tests/depth_scoring_test.cpp` verifica que:

- o `1920x2160` com 32.818 bindings vence os candidatos reais do log do ETS2;
- um depth ATS esperado em `2400x1350` vence recursos de menu `1920x1080`;
- 140 bindings de menu nao atingem a confianca minima;
- recursos MSAA nao sao aceitos para a copia usada pelo SSAO.
