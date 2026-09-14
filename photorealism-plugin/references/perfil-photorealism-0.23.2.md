# Perfil de tom 0.23.2

Estado: entregue. Ainda nao rodou no jogo. Substitui `perfil-photorealism-0.23.1.md`.

## O perfil como unica fonte

O usuario pediu que as chaves de `[profile.photorealism.0.23.0]` fizessem todo o trabalho, porque as
outras secoes do cfg brigavam com elas. O que saiu e para onde foi:

| Secao | Destino |
|---|---|
| `base.0.1.2`, `module.visual.0.2.0`, `module.rain_overcast.0.3.0` | removidas; o grade vem do conjunto de tom |
| `module.user.0.20.0` | removida; o menu grava direto nas chaves do perfil |
| `module.condition_adaptation.0.19.0` | removida; a deteccao de noite fica interna e so pesa `tonemap_night_exposure_<n>` |
| `depth.0.6.4` | removida; valores internos (usados pelo SSAO e pelo preview do Insert) |
| `module.ssao.0.7.0`, `module.ssao_refinement.0.8.0`, `module.ssao_interior.0.9.0` | removidas; SSAO roda com os valores de antes, internos, e `ssao_intensity` multiplica |
| `module.temporal.0.10.0` | removida; `taa` liga e desliga, os parametros ficam internos |

Continuam no cfg, fora do perfil: `plugin`, `module.fsr.0.21.0`, `native_aa.0.12.2`,
`module.bloom.0.17.0`, `module.scene_observer.0.18.0`.

Campos do grade que o perfil nao tem ficam neutros: piso de preto, joelho de altas luzes, matiz e
vinheta. Era assim desde a 0.23.0 com o perfil ligado.

## Iluminacao e conjunto de tom

`lighting_method` escolhe o conjunto: A (0) usa o 1, B (1) o 2, C (2) o 3, D (3) o 4. E a mesma
leitura da 0.23.0 (conjunto = metodo + 1). O conjunto 5 continua no cfg, copiado como estava, e
nenhuma iluminacao o usa.

A pagina Cores / Tom mostra e edita o conjunto da iluminacao escolhida; trocar a iluminacao carrega o
conjunto dela sem perder o que foi editado nos outros. O Salvar grava so as chaves
`tonemap_<controle>_<n>` que mudaram.

## Chaves e controles

| Chave | Controle | Efeito hoje |
|---|---|---|
| `lighting_method` | lista: Iluminacao A, B, C, D (padrao) | escolhe o conjunto de tom |
| `global_quality` | lista: Qualidade alta, media, baixa | sem equivalente no plugin |
| `taa` | lista, na ordem das telas de referencia: Desligado, temporal, temporal nitido, DLAA, DLSS qualidade, equilibrado, desempenho | 0 desliga o resolve temporal do plugin; os demais ligam. 1 e 2 dao o mesmo resolve; 3 a 6 nao tem suporte |
| `fxaa` | botao | pendente: FXAA (0.23.3) |
| `sharpness`, `sharpen_edges` | sliders 0-10 inteiros | nitidez e contraste local do grade, divididos por 10 |
| `use_motion_blur`, `motion_blur_intensity` | botao, slider | pendente: vetores de movimento |
| `use_half_res_ssao` | botao | sem efeito: o SSAO do plugin roda sempre em resolucao cheia |
| `ssao_preset`, `ssao_detail_quality` | listas | sem equivalente no plugin |
| `ssao_intensity` | slider | multiplica as duas intensidades do SSAO |
| `lighting_interior`, `use_interior_lighting` | slider, botao | pendente: HDR do jogo |
| `use_default_mirrors` | botao | pendente: alvo do espelho |
| `use_default_rain`, `vegetation_leaves_thickness`, `vegetation_grass_thickness` | botao, sliders | exigem troca de shader |
| `use_sss` | botao | pendente: descoberta do G-buffer |
| `surface_albedo_saturation`, `roads_normal_intensity`, `roads_default_normals` | sliders, botao | pendente: descoberta do G-buffer |
| `tonemap_*_<n>` | sliders em Cores / Tom | temperatura, exposicao, saturacao, contraste, vibracao, sombras, altas luzes, pretos, brancos e exposicao noturna aplicam; pre-exposicao, pre-contraste e contraste dinamico pendentes do HDR do jogo |
| `taa_level`, `dlss_preset`, `color_preset`, `color_preset_extra_brightness`, `tonemap_operator`, `tonemap_operator_a`, `hide_show_key` | so no cfg | nao aparecem nas telas de referencia; sem equivalente no plugin |

O log lista, a cada leitura do cfg, as chaves sem efeito agrupadas pelo motivo.

## O que nao se sabe

- Os nomes das opcoes 2 e 3 da lista de SSAO: as telas de referencia so mostram "SSAO Soft". Os
  nomes de detalhe (alto, medio, baixo) seguem a lista de qualidade, e tambem nao foram vistos.
- A ordem dos valores de `taa`: a lista segue a ordem da tela de referencia, contando do zero. O cfg de
  referencia traz `taa=4` junto com `dlsspreset2=2`, coerente com DLSS qualidade numa placa NVIDIA.
- O botao "Distance Screen Space Shadows" das telas de referencia nao tem chave no cfg fornecido e
  ficou de fora.

## Menu

Paginas no molde das telas de referencia: inicio com as duas listas e os botoes de pagina, separador,
Mostrar / esconder (Ctrl+P) e Restaurar padroes; cada pagina com Voltar. Escolha entre opcoes e lista
suspensa, chave 0/1 e botao, slider so para faixa continua. Nenhuma linha esmaecida. O painel se
ajusta a altura da pagina. Upscale FSR tem pagina propria, com o estado do upscale no topo.

O menu foi rasterizado fora do jogo a partir da lista de desenho real (Wine, harness no scratchpad):
inicio, listas abertas e as oito paginas, e a escolha "Iluminacao B" pelo clique trocou a exposicao
para 0.25.
