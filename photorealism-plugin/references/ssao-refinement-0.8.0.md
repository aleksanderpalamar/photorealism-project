# Refinamento SSAO photorealista 0.8.0

## Objetivo visual

Transformar o SSAO experimental aprovado em uma oclusao mais uniforme e
fisicamente plausivel, sem alterar a coloracao consolidada e sem apagar fontes
luminosas ou reflexos importantes para chuva e conducao noturna.

## Amostragem espacial

A 0.7.0 usava quatro direcoes cardeais no anel interno e quatro diagonais no
anel externo. A 0.8.0 completa as direcoes ausentes:

- oito direcoes no anel interno de `0.45` do raio;
- oito direcoes no anel externo de `1.00` do raio;
- 16 amostras no total;
- normalizacao reduzida pela metade para preservar a intensidade media.

O raio, intensidade, bias, fade e rejeicao de bordas continuam vindo de
`[module.ssao.0.7.0]`. Portanto, o novo modulo refina a distribuicao e nao
substitui a calibracao aprovada.

## Protecao de luzes

SSAO representa bloqueio de iluminacao ambiente, nao o desaparecimento de luz
direta ou emissiva. A luminancia e calculada depois da conversao sRGB para
linear. Entre `highlight_start=0.55` e `highlight_end=0.95`, a influencia do
SSAO diminui gradualmente. Em altas luzes ela conserva
`highlight_ao_floor=0.35` da influencia original.

O resultado esperado e preservar melhor:

- farois e luzes auxiliares;
- flares e lampadas urbanas;
- gotas claras no para-brisa;
- reflexos especulares no asfalto molhado;
- nuvens e superficies brancas fortemente iluminadas.

## Comparacao

Com `[module.ssao_refinement.0.8.0] enabled=false`, o shader usa novamente as
oito amostras e a composicao integral da 0.7.0. Isso permite comparacao A/B por
edicao do CFG e `End`, mantendo todas as demais camadas identicas.
