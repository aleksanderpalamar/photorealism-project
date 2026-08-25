# SSAO experimental 0.7.0

## Objetivo

Adicionar a primeira oclusao ambiente espacial ao Photorealism Plugin sem
alterar o shader visual consolidado. A etapa deve confirmar intensidade,
estabilidade, custo e ausencia de halos antes de aumentar qualidade ou raio.

## Pipeline

1. copia nao destrutiva do depth principal;
2. tratamento visual aprovado em uma textura intermediaria sRGB;
3. reconstrucao aproximada de posicao e normal em espaco de camera;
4. oito amostras de profundidade ao redor do pixel;
5. rejeicao de ceu, amostras invalidas e saltos alem do limite espacial;
6. composicao da visibilidade sobre a imagem tratada.

Quando o depth ainda nao foi descoberto ou algum recurso falha, o passo 2 e
desenhado diretamente no backbuffer e a imagem da versao anterior e preservada.

## Perfil inicial

- `radius=0.8` metro;
- `intensity=0.28`;
- `bias=0.04`;
- `fade_start=30.0` metros;
- `fade_end=70.0` metros;
- `edge_rejection=1.5` vezes o raio;
- oito amostras fixas em dois aneis.

## Diagnostico

O modo `ssao-visibility` e o sexto estado do ciclo de `Insert`, depois das
normais reconstruidas. Branco significa visibilidade total; tons cinza indicam
oclusao. O teste deve observar principalmente contato dos pneus com o chao,
degraus, para-choques, encontros entre muros e piso e detalhes da cabine.

Contornos escuros acompanhando toda a silhueta, manchas em ceu ou vegetacao e
ruido generalizado nao sao resultados aceitaveis. O custo GPU e registrado pela
mesma telemetria de dez segundos e engloba os dois passes.
