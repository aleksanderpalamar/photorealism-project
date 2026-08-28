# RTGI 0.12.1 - ray march de raio unico em screen-space

Data: 2026-08-28
Escopo: Photorealism 0.12.1, branch `feat/screen-space-ray-traced-global-ilumination`
Estado: raios tracados; resultado ainda nao composto na imagem

## O que muda

A 0.12.0 entregou a fundacao com `resolve_indirect()` devolvendo zero. Esta
versao preenche esse corpo: um raio por pixel, marchado em view-space e
projetado de volta para a tela a cada passo.

O resultado **nao** e composto na cena. Ele preenche o buffer RTGI_RAW em meia
resolucao e se verifica pelas debug views. Compor e 0.12.2. A propriedade de
"nao pode piorar a imagem" continua valendo por construcao: nada le o buffer.

## A projecao inversa

O ray march precisa saber onde o raio caiu na tela. `depth_view_space.hlsli`
ganhou `project_view_position`, inverso exato de `reconstruct_view_position`:

```
view.x = ndc.x * z / proj.x   ->   ndc.x = view.x * proj.x / z
uv = ((ndc.x + 1) / 2, (1 - ndc.y) / 2)
```

Ela mora no mesmo header, e nao no `rtgi.hlsl`, porque separar as duas metades
da mesma transformacao em arquivos diferentes e exatamente como a duplicacao
que a 0.12.0 veio desfazer comecou.

Como nenhum dos shaders aprovados a chama, o bytecode dos quatro continuou
identico ao de `5e05211` -- verificado com `tools/shader_check.sh`.

## A marcha

```
origem = posicao_view + normal * NormalBias
t      = RangeMin
passo  = (RangeMax - RangeMin) / MaxSteps

repetir MaxSteps vezes:
    t += passo
    p = origem + direcao * t
    se p.z <= NearPlane          -> parar (atras do plano proximo)
    uv = project_view_position(p)
    se uv fora de [0,1]          -> saiu da tela
    raw = depth em uv
    se raw <= epsilon            -> ceu
    delta = p.z - linearize(raw)
    se 0 < delta < HitThickness  -> ACERTO
```

`HitThickness` existe porque screen-space nao conhece a profundidade real do
que o raio atinge: so conhece o depth da superficie visivel naquele texel. Sem
o limite, qualquer coisa atras da geometria contaria como acerto e a luz
vazaria por tras das paredes.

`NormalBias` desloca a origem ao longo da normal para o raio nao acertar a
propria superficie no primeiro passo.

## Confianca: a diferenca entre vazio e desconhecido

| desfecho | confianca | indireta |
|---|---|---|
| acerto | 1.0 | cor rebatida x `dot(N, dir)` |
| raio termina em texel de ceu | 1.0 | `SkyAmbient * saturate(dir.y)` |
| raio sai da tela | 0.0 | `SkyAmbient * saturate(dir.y)` |
| passos esgotados | 0.0 | 0 |

Sair da tela nao e o caso vazio, e o caso *desconhecido*: screen-space
simplesmente nao tem a informacao. Registrar isso separadamente e o que vai
permitir a acumulacao temporal da 0.12.3 confiar mais em quem sabe, em vez de
tratar ignorancia como ausencia de luz.

Na debug view `confidence` isso aparece como valor alto no centro da tela e
baixo nas bordas -- a assinatura correta da tecnica.

## Amostragem

Direcao uniforme no hemisferio da normal, com o peso `dot(N, dir)` aplicado
explicitamente, exatamente como o documento da tecnica escreve. Cosine-weighted
teria variancia menor, mas ai o peso explicito viraria `cos²` e escureceria
demais os bounces rasantes; a troca fica para a 0.12.2, junto da divisao por
`rayCount`.

A semente varia por pixel **e por `FrameIndex`**: os raios ja mudam a cada
frame, que e de onde a 0.12.3 vai extrair amostras de graca.

## Espaco linear

`RtgiConstants` cresceu de 64 para 80 bytes para receber `HitThickness`,
`NormalBias` e `InputNeedsSrgbDecode`. Este ultimo nao existia e era inofensivo
enquanto o shader nao lia cor nenhuma; agora e correcao. Sem decodificar a cor
rebatida para linear, o bounce seria calculado sobre valores sRGB e o GI sairia
claro demais nas sombras.

## Debug views

Com o Insert na posicao 6, **Page Down** cicla:

```
normals -> rays -> hit_distance -> raw_gi -> confidence -> normals
```

`temporal_gi` fica de fora ate a 0.12.3 existir, e `final` nao e diagnostico. O
ciclo e a funcao pura `next_rtgi_preview_debug`, testada, para nao virar uma
cadeia de literais no `postprocess.cpp`. O passe de trabalho continua obedecendo
ao `debug=` do cfg; so o preview obedece ao Page Down.

`hit_distance` e a view que falsifica a projecao: se `project_view_position`
estivesse errada, os raios sairiam da tela no primeiro passo e a tela inteira
ficaria no valor de miss. O esperado e gradiente coerente, mais curto perto de
paredes e do chao.

## Limitacoes conhecidas

1. **A cor da cena inclui a HUD.** `scene_texture_` e uma copia do backbuffer no
   Present, ou seja o frame do jogo ja com interface. Luz rebatida pode pegar
   pixels de HUD. Nao ha sinal confiavel para mascarar isso hoje: a textura de
   cena pre-UI depende da prova de composicao, ainda em investigacao na branch
   `fsr-0.7.2-tiles`.
2. **Passo fixo em view-space** e grosso perto da camera e desperdicado longe.
   E o que o traversal Hi-Z da 0.12.5 resolve.
3. **`saturate(dir.y)` e "cima" em view-space**, que inclina junto com a camera.
   Aceitavel no ETS2, onde se dirige praticamente nivelado.
4. **Um raio por pixel, sem denoise: `raw_gi` parece ruido.** E esperado, nao
   defeito. A 0.12.3 (temporal) e a 0.12.4 (bilateral) sao o que tornam o sinal
   usavel.

## Custo

`PSRtgi` foi de 336 para 590 instrucoes. Ate 12 leituras de depth por pixel em
960x540, mais uma leitura de cor no acerto. Com `enabled=false`, que continua
sendo o padrao, nenhum draw acontece.

## Proximas fases

| versao | entrega |
|---|---|
| 0.12.2 | GI difusa multi-raio, `MaxIndirectLuma`, composicao com `gi_intensity` |
| 0.12.3 | acumulacao temporal com rotacao de raios e `normal_rejection` |
| 0.12.4 | denoiser bilateral depth-aware e normal-aware |
| 0.12.5 | traversal Hi-Z sobre mips de depth |
| 0.12.6 | qualidade adaptativa e presets Low/Medium/High |
