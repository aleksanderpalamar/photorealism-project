# Perfil de tom 0.23.1

Estado: entregue. Ainda nao rodou no jogo. Continua `perfil-photorealism-0.23.0.md`.

## Conjuntos escolhiveis

Os cinco conjuntos de tom do cfg de referencia estao no cfg empacotado, com os valores copiados como
estao. `tonemap_set` virou ajuste de modulo da secao `[profile.photorealism.0.23.0]`, e o menu (aba
Perfil, "Conjunto de tom") o grava ali. Trocar o conjunto recompoe o grade na hora a partir do cfg em
disco, como o botao do perfil ja fazia; a recomposicao so acontece quando o numero arredondado muda,
entao arrastar o slider dentro do mesmo conjunto nao le o disco a cada quadro.

| Conjunto | Exposicao | Saturacao | Contraste | Vibracao | Sombras | Altas luzes | Pretos | Brancos | Noturna | Pre-exp. | Pre-contr. |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | -0.09 | 0.82 | 0.92 | -0.15 | 0 | 0 | 0 | 0.03 | 2.00 | -1.00 | 0.42 |
| 2 | 0.25 | 0.97 | 0.93 | 0 | -0.06 | 0.06 | -0.02 | -0.13 | 0 | 0 | 0.26 |
| 3 | 0 | 1.01 | 1.00 | 0 | 0 | 0 | 0 | 0 | 0 | 0.10 | 0 |
| 4 | -0.06 | 1.00 | 0.99 | 0 | -0.01 | -0.07 | 0 | -0.01 | 0 | -- | -- |
| 5 | 0 | 1.00 | 1.00 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

Temperatura 6500 K e contraste dinamico 1.00 nos cinco. O conjunto 4 nao tem pre-exposicao,
pre-contraste nem contraste dinamico no cfg de referencia; no photorealism-plugin eles ficam neutros.

## Exposicao noturna

`tonemap_night_exposure_<n>` agora aplica: a exposicao enviada ao shader e
`exposure + night_exposure * peso_de_noite`, em EV. O peso de noite e o da adaptacao por condicao
(0.19.0), que ja separa noite de dia pela mediana suavizada. **Suposicao**: que no cfg de referencia a
exposicao noturna seja um acrescimo em EV que cresce com a noite. So o conjunto 1 usa (+2 EV).

Para isso a adaptacao por condicao passou a medir a cena tambem com a cor travada pelo perfil; a trava
continua impedindo que ela mexa em temperatura e matiz. Com o observador de cena ou a adaptacao
desligados, nao ha peso de noite e o log avisa que a exposicao noturna fica sem efeito. O peso segue a
suavizacao de 180 s, entao a exposicao sobe devagar ao anoitecer.

## Chaves lidas e ainda sem efeito

Lidas do cfg, mostradas em cinza na aba Perfil (rotulo com `*`), nunca gravadas pelo menu, e listadas
no log agrupadas pelo motivo.

| Chave no photorealism-plugin | Valor | Motivo |
|---|---|---|
| `tonemap_pre_exposure_<n>`, `tonemap_pre_contrast_<n>`, `tonemap_dynamic_contrast_<n>` | por conjunto | pendente: HDR do jogo |
| `lighting_interior`, `use_interior_lighting` | 0.17, 1 | pendente: HDR do jogo |
| `surface_albedo_saturation`, `roads_normal_intensity`, `roads_default_normals`, `use_sss` | 1.07, 3, 1, 1 | pendente: descoberta do G-buffer |
| `use_default_mirrors` | 0 | pendente: alvo do espelho |
| `use_motion_blur`, `motion_blur_intensity` | 1, 10 | pendente: vetores de movimento |
| `vegetation_leaves_thickness`, `vegetation_grass_thickness`, `use_default_rain` | 0, 1, 0 | exige troca de shader |
| `fxaa` | 1 | pendente: FXAA (0.23.2) |
| `use_half_res_ssao` | 0 | ja atendido: o SSAO do plugin roda em resolucao cheia |
| `taa`, `taa_level`, `dlss_preset`, `hide_show_key`, `color_preset`, `color_preset_extra_brightness`, `tonemap_operator`, `tonemap_operator_a`, `lighting_method`, `ssao_preset`, `ssao_detail_quality`, `global_quality` | 4, 1, 2, 520, 0, 0, 0, 7, 3, 0, 0, 0 | sem equivalente no plugin |

Nomes do cfg de referencia para os do photorealism-plugin: `Lighting_Interior` -> `lighting_interior`,
`UseInteriorLighting` -> `use_interior_lighting`, `surface_albedosaturation` ->
`surface_albedo_saturation`, `roads_normalintensity` -> `roads_normal_intensity`,
`roads_defaultnormals` -> `roads_default_normals`, `UseSSS` -> `use_sss`, `UseDefaultMirrors` ->
`use_default_mirrors`, `usemotionblur` -> `use_motion_blur`, `motionblurintensity` ->
`motion_blur_intensity`, `vegetation_leavesthickess` -> `vegetation_leaves_thickness`,
`vegetation_grassthickness` -> `vegetation_grass_thickness`, `UseDefaultRain` -> `use_default_rain`,
`fxaa2` -> `fxaa`, `UseHalfResSSAO` -> `use_half_res_ssao`, `taalevel` -> `taa_level`, `dlsspreset2` ->
`dlss_preset`, `hideshowkey` -> `hide_show_key`, `colorpreset` -> `color_preset`,
`ColorPreset_ExtraBrightness` -> `color_preset_extra_brightness`, `Tonemap_Tonemap` ->
`tonemap_operator`, `Tonemap_TonemapA` -> `tonemap_operator_a`, `LightingMethod` -> `lighting_method`,
`SSAO_Preset` -> `ssao_preset`, `SSAO_DetailQuality` -> `ssao_detail_quality`, `Global_Quality` ->
`global_quality`.

## Cfg sem comentarios

O cfg empacotado nao tem mais comentarios, a pedido do usuario. Os valores nao mudaram. A justificativa
medida de cada numero continua no CHANGELOG, nestas referencias e no historico do git (o cfg comentado
e o da 0.23.0). A guarda que exigia a ressalva do bloom dentro do cfg passou a exigir a ressalva na
linha de log do modulo, que diz "licenca artistica; so o limiar e medido".
