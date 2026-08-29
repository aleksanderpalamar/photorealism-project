# RTGI 0.13.2 - o GI passa a alterar a imagem

Data: 2026-08-29
Escopo: Photorealism 0.13.2, branch `feat/screen-space-ray-traced-global-ilumination`
Estado: GI composto na cena; ruido temporal ainda nao tratado

## O que muda

A 0.12.1 tracava raios que nao chegavam a lugar nenhum: o buffer de meia
resolucao era preenchido e so as debug views o liam. Esta versao compoe, e para
isso precisou consertar antes uma coisa que ninguem tinha medido.

## O ponto cego da cabine

Os parametros vieram do documento da tecnica com escala externa em mente --
asfalto, paredes, postos, edificios. A conta que faltava fazer:

```
passo = (range_max - range_min) / max_steps = (15.0 - 0.5) / 12 = 1,21 m
```

e `travelled` comeca em `range_min` com o laco somando **antes** de amostrar.
As doze amostras caiam em:

```
1,71  2,92  4,13  5,33  6,54  7,75  8,96  10,17  11,38  12,58  13,79  15,0
```

Nada entre 0,5 e 1,71 m. A cabine inteira vive nessa faixa: banco a ~0,5 m,
painel e GPS a ~0,7 m, painel de porta a ~0,7 m, para-brisa a ~1 m. **Um raio
saindo do painel pulava a cabine no primeiro passo** e ia amostrar o asfalto do
lado de fora. Compor com esses numeros produziria GI externo e nada dentro --
justamente o primeiro caso de uso que o documento lista.

## Progressao geometrica, e nao perfil de interior

A saida obvia era espelhar `[module.ssao_interior.0.9.0]`: um perfil de curto
alcance misturado por distancia de camera. Foi descartada em favor de trocar a
distribuicao dos passos:

```
razao = (range_max / range_min) ^ (1 / max_steps)
t_k   = range_min * razao^k
```

Com `range_min=0.10`, `range_max=15.0` e os mesmos 12 passos, a razao vale
1,5182 e as amostras caem em:

```
0,15  0,23  0,35  0,53  0,81  1,22  1,86  2,82  4,29  6,51  9,88  15,0
       ^------ seis dentro da cabine ------^
```

Seis amostras no interior e o exterior ainda alcancando 15 m, com o mesmo custo
de passos. E sem seccao nova no cfg, sem heuristica de "isto e cabine?" e sem o
efeito colateral que o perfil por distancia teria: um carro a 3 m na estrada
tambem contaria como interior e perderia o alcance longo.

| | passo fixo (0.12.1) | geometrico (0.13.2) |
|---|---|---|
| primeira amostra | 1,71 m | 0,15 m |
| amostras <= 1,5 m | 0 | 6 |
| alcance final | 15,0 m | 15,0 m |
| passos | 12 | 12 |

`src/rtgi_config.hpp` ganhou `rtgi_step_ratio`, `rtgi_sample_distance` e
`rtgi_samples_within`, puras e testadas no Linux. A terceira existe para a
cobertura da cabine virar invariante em vez de aritmetica refeita a mao:

```cpp
assert(rtgi_samples_within(0.10f, 15.0f, 12, 1.5f) >= 4);
```

Com `range_min=0.5` esse teste devolve 3 e falha. E o que impede a regressao de
voltar por edicao de cfg ou por "arredondar" o valor.

## Espessura deixou de ser fixa

`hit_thickness` passou de valor absoluto a **teto**. A ambiguidade de
profundidade que a marcha introduz *e* o comprimento do passo: nada se sabe
sobre o que esta entre duas amostras. Entao:

```
espessura_k = min(passo_k, HitThickness)
```

Perto isso da ~0,05 m, o que impede a luz de vazar pelo painel para dentro do
motor. Longe, o teto de 0,5 m impede que uma fatia de 5 m aceite qualquer coisa
como acerto -- que seria vazamento de luz por tras de predios inteiros.

O efeito colateral aceito: muitos raios longos passam a nao achar nada e
devolvem zero. E o desfecho conservador correto; GI a 10 m importa menos que
nao inventar luz.

## Quatro raios, cosseno e firefly

- **`ray_count` deixou de ser inerte.** Quatro raios por pixel, somados e
  divididos. O ruido cai com a raiz do numero de amostras, entao quatro cortam
  o desvio pela metade -- ainda nao e limpo, e por isso a 0.13.3 e a 0.13.4
  existem;
- **amostragem cosine-weighted.** `cos_theta = sqrt(random.x)`, e o
  `dot(N, dir)` explicito **saiu** de `march_ray`. Com o peso ja no PDF,
  mante-lo daria `cos²`, que escurece demais os bounces rasantes -- que sao
  exatamente os que carregam o color bleeding de parede e de painel. A troca
  estava anotada como pendente desde a 0.12.1;
- **`max_indirect_luma` aplicado por raio, antes da media.** E rejeicao de
  firefly: um farol, o sol num vidro ou um pixel de HUD domina a media de
  quatro raios e vira um ponto branco piscando. Depois da media nao haveria o
  que proteger, o estrago ja estaria diluido em todos os quatro.

As debug views `rays` e `hit_distance` passam a mostrar o **primeiro** raio --
sao diagnostico por raio, e promediar direcao nao produz nada interpretavel.
`raw_gi` e `confidence` mostram o acumulado, que e o que de fato e composto.

## O passe de composicao

`PSRtgiCompose`, segundo entry point do proprio `rtgi.hlsl`, reaproveitando
cbuffer e tratamento de sRGB. Le a cor de cena e o buffer de GI, soma em espaco
linear com `gi_intensity` e devolve ao espaco da copia de cena.

A cadeia ganhou **uma variavel, nao uma ramificacao**:

```cpp
ID3D11ShaderResourceView* grading_source = scene_view_;
if (rtgi_active) {
    render_rtgi_pass(rtgi_target_, rtgi_width_, rtgi_height_);
    ID3D11ShaderResourceView* composed = render_rtgi_compose_pass(description);
    if (composed != nullptr) { grading_source = composed; }
}
```

Os tres ramos do grading (`temporal_active`, `ssao_active` e o `else`) passaram
a ler `grading_source`. Com o RTGI desligado, ou com o alvo de composicao
indisponivel, ele vale `scene_view_` e a cadeia e byte a byte a de antes.

**`photorealism.hlsl` nao foi tocado.** Foi a razao de a composicao virar passe
proprio em vez de mais um trecho do grading: o shader calibrado desde a 0.1.2
continua com o hash pinado, e o GI ainda assim entra antes dele, que era o
requisito declarado no documento -- exposure, contraste e LUT alcancam a luz
indireta tambem.

Prova em bytecode: os quatro shaders aprovados sairam **byte-identicos** ao
`HEAD` -- 2367 linhas de disassembly iguais dos dois lados.

## Custo

| entry point | 0.12.1 | 0.13.2 |
|---|---|---|
| `PSRtgi` | 591 | 688 |
| `PSRtgiCompose` | - | 60 |

O bytecode nao quadruplica porque o laco de raios e dinamico (`[loop]`): o que
multiplica e a execucao, nao o codigo. Estimativa de ~2 ms para a marcha mais
~0,1 ms para a composicao em resolucao cheia. Se passar disso, o recuo e
`ray_count=2`, e nao desligar o modulo.

## Limitacoes que continuam

1. **A HUD entra na cor de cena.** `scene_texture_` e copia do backbuffer no
   Present, ja com interface. De noite piora, porque a HUD passa a ser a coisa
   mais clara da tela. Depende da prova de composicao da branch
   `fsr-0.7.2-tiles`;
2. **Upsample bilinear** do buffer de meia resolucao borra o GI atraves de
   bordas de geometria. O upsample depth-aware entra com o bilateral da 0.13.4;
3. **Ruido temporal.** Quatro raios sem denoise ainda cintilam. E o motivo de o
   modulo continuar nascendo desligado;
4. **`saturate(dir.y)` e "cima" em view-space**, que inclina com a camera.

## Proximas fases

| versao | entrega |
|---|---|
| 0.13.3 | acumulacao temporal com rotacao de raios e `normal_rejection` |
| 0.13.4 | denoiser bilateral depth-aware e normal-aware, e upsample ciente de depth |
| 0.13.5 | traversal Hi-Z sobre mips de depth |
| 0.13.6 | qualidade adaptativa e presets Low/Medium/High |
| 0.13.7 | recalibracao do SSAO sobre o depth de camera correto |
