# RTGI 0.13.2.1 - o raio que escapa deixa de devolver preto

Data: 2026-08-30
Escopo: Photorealism 0.13.2.1, branch `feat/screen-space-ray-traced-global-ilumination`
Estado: correcao da 0.13.2; ruido temporal ainda nao tratado

## O que o teste em jogo mostrou

A 0.13.2 compos. Do lado de fora apareceu granulado, que e a prova de que o
passe de composicao rodou -- sem ele nao haveria nada. Dentro da cabine, nada.

O quadro que fechou o diagnostico foi um **tunel**: paredes de concreto branco
iluminadas a 3-4 m em volta inteira, a geometria mais favoravel a GI que o jogo
oferece, com o painel preto chapado. E preto **sem granulado**, enquanto o
asfalto la fora granulava.

Ruido ausente onde deveria haver ruido nao e denoise faltando. Quatro raios com
resultados diferentes produzem variancia; a ausencia total de variancia so tem
uma causa possivel, que e os quatro devolverem o mesmo valor. Zero.

## Os quatro desfechos

`march_ray` termina de quatro maneiras, e so uma e acerto:

| desfecho | ate a 0.13.2 | confianca |
|---|---|---|
| acerto numa superficie | cor da cena | 1 |
| saiu da tela | `SkyAmbient * saturate(dir.y)` | 0 |
| ceu de verdade (`raw_depth == 0`) | `SkyAmbient * saturate(dir.y)` | 1 |
| passos esgotados / plano proximo | **zero duro** | 0 |

E `config/photorealism-plugin.cfg` trazia `sky_ambient=0.0`.

Somando as duas linhas: **tres dos quatro desfechos devolviam preto**, e o
quarto era o unico que podia devolver luz. O preto virou a resposta do shader
para "nao sei", que e o vies exatamente invertido -- um raio que atravessou
todo o alcance util sem encontrar superficie nenhuma passou por espaco aberto,
e espaco aberto e claro.

## Por que isso custa a cabine, e nao apenas escurece um pouco

`depth_view_space.hlsli` termina a reconstrucao com:

```hlsl
if (normal.z > 0.0)
{
    normal = -normal;
}
```

Toda normal visivel aponta para a camera -- tem que apontar, senao a superficie
seria backface. Consequencia: o hemisferio de amostragem de uma superficie
virada para a camera e o cone **entre ela e o olho**. No painel, isso e ar
vazio.

Os dois caminhos que os raios do painel tomam:

- **raios tipicos** (cosseno concentra perto da normal) andam para tras, em
  direcao a camera. O painel esta a ~0,7 m e `near_plane=0.1`, entao apos
  ~0,6 m de marcha eles cruzam o plano proximo e caem no `break` -- por volta
  da sexta das doze amostras;
- **raios rasantes** sobem em direcao ao para-brisa. Ficam na tela, mas a
  superficie sob a projecao deles e a estrada, a dezenas de metros. `delta`
  sai negativo nos doze passos, nenhum acerto, laco esgotado.

Os dois desfechos devolviam zero. Quatro zeros tem media exatamente zero:
preto perfeitamente liso, sem nem o granulado que teria denunciado o problema.

O tunel nao ajuda porque as paredes dele, por mais claras que sejam, tambem
estao **atras** do painel em view-space.

## A correcao

Um termo unico, compartilhado pelos tres desfechos de nao-acerto:

```hlsl
float3 ambient_escape(float3 direction)
{
    return SkyAmbient * saturate(direction.y);
}
```

O `break` do plano proximo deixa de ser um caminho separado: ele cai no mesmo
`return` do fim do laco, porque e a mesma situacao -- o raio saiu do volume
util sem aprender nada.

A confianca continua em zero nesses desfechos, de proposito. E o desfecho em
que o screen-space admite nao saber, e e disso que a rejeicao temporal da
0.13.3 precisa para confiar menos neles do que num acerto real.

`saturate(direction.y)` e um modelo de ceu barato: quem olha para cima recebe
mais. O "cima" e o da camera e inclina com ela -- limitacao herdada da 0.12.1 e
ainda nao resolvida.

## O valor de sky_ambient

`0.0` -> `0.25`, no cfg e em `default_rtgi_settings()`.

Nao e a radiancia de um ceu. E a radiancia atribuida a uma direcao
**desconhecida**, e em jogo a maioria das direcoes desconhecidas esta
parcialmente ocluida: cabine, tunel, viaduto, vao entre predios, sombra de
predio. Um quarto de um ceu encoberto tipico e o lado conservador dessa conta.

Ordem de grandeza no painel: com `dir.y` medio perto de 0,5 e
`gi_intensity=0.15`, a soma em espaco linear fica em ~0,019 -- de preto para um
cinza escuro visivel, e nao um lavado. No exterior a diferenca e desprezivel,
porque la os raios acertam geometria de verdade e o escape e minoria.

Se o interior sair escuro demais, subir; se a cabine ficar leitosa, descer. E
edicao de cfg, sem recompilar. **Zero nao e um ajuste, e o bug de volta**, e
por isso `tools/validate.sh` barra o valor por nome.

## O que a 0.13.2 entregou de fato

A marcha geometrica consertou um ponto cego de amostragem que era real e
continua consertado: sem ela nenhuma amostra caia abaixo de 1,71 m. Mas ela nao
era a unica coisa entre o RTGI e o interior, e o criterio de aceite escrito no
plano da 0.13.2 -- "painel e bancos deixando de ser preto chapado" -- nao tinha
como ser atingido naquela versao. O criterio estava certo; a versao a que ele
foi atribuido, nao.

## O limite que continua, agora medido

Isto da a cabine um **piso de ambiente modulado pela direcao do raio**, que e o
efeito de penumbra. Nao da color bleeding do exterior para dentro.

Uma superficie virada para a camera so enxerga o que esta **ao lado dela, em
profundidade parecida**. A estrada vista pelo para-brisa nao esta: esta atras do
painel, fora do hemisferio dele. Nenhum ajuste de parametro alcanca isso em
screen-space puro.

O caso do GPS e do painel a noite (ver ROADMAP) **nao** esbarra nesse limite --
tela e painel estao lado a lado em profundidade parecida, que e justamente a
geometria que o screen-space alcanca. O obstaculo que resta ali continua sendo
o ruido, ou seja a 0.13.3 e a 0.13.4.

## Regressoes travadas

Em `tools/validate.sh`:

- `sky_ambient=0.25` pinado no cfg, e `^sky_ambient=0(\.0+)?$` barrado por nome
  com mensagem propria;
- `float3 ambient_escape(float3 direction)` exigido em `rtgi.hlsl`;
- a contagem de `result.indirect = ambient_escape(direction);` tem que ser
  exatamente **3**. Se um desfecho voltar a devolver preto, a contagem cai e a
  validacao falha;
- hash do cfg repinado.

Em `tests/rtgi_config_test.cpp`:

```cpp
static_assert(clamped_defaults.sky_ambient > 0.0f);
static_assert(clamped_defaults.sky_ambient == 0.25f);
```

O primeiro e a invariante; o segundo e o pino de calibracao.

## Verificacao em jogo

Page Up faz o A/B. O que precisa mudar, e onde:

- **cabine, de dia**: painel, volante e portas saindo do preto chapado para um
  cinza escuro com gradiente -- mais claro nas superficies viradas para cima.
  E o caso que a 0.13.2 nao conseguia nem em principio;
- **tunel**: o mesmo, e mais evidente, porque fora da cabine a tela toda e
  clara;
- **exterior**: praticamente sem mudanca. Se o exterior lavar, `sky_ambient`
  esta alto demais.

Sinal de que passou do ponto: cabine leitosa, sem contraste entre o que esta
virado para cima e o que esta virado para baixo.
