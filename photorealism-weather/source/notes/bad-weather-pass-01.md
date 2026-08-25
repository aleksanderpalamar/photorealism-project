# Mau tempo diurno — passe 01

Escopo: `default.bad.05` até `default.bad.31`.

O objetivo é preservar a nitidez observada nas gotas e no spray, reduzir o véu
luminoso do pós-processamento e enfraquecer sombras solares sob nuvens. Este passe
não muda a simulação ou a frequência da chuva.

## Amanhecer chuvoso — `.05` a `.13`

| Campo | Multiplicador |
|---|---:|
| `ambient` | 0.98 |
| `sun_shadow_strength` | 0.82 |
| `fog_density` | 1.10 |
| `color_saturation` | 0.97 |
| `contrast` | 0.98 |
| `bloom_threshold` | 1.25 |
| `bloom_limit` | 0.88 |
| `bloom_intensity` | 0.58 |
| `sunshaft_color` | 0.65 |

## Mau tempo diurno — `.14` a `.22`

| Campo | Multiplicador |
|---|---:|
| `ambient` | 0.98 |
| `sun_shadow_strength` | 0.80 |
| `fog_density` | 1.06 |
| `color_saturation` | 0.97 |
| `contrast` | 0.98 |
| `bloom_threshold` | 1.30 |
| `bloom_limit` | 0.85 |
| `bloom_intensity` | 0.52 |
| `sunshaft_color` | 0.60 |

## Entardecer chuvoso — `.23` a `.31`

| Campo | Multiplicador |
|---|---:|
| `ambient` | 0.97 |
| `sun_shadow_strength` | 0.82 |
| `fog_density` | 1.08 |
| `color_saturation` | 0.96 |
| `contrast` | 0.97 |
| `bloom_threshold` | 1.25 |
| `bloom_limit` | 0.86 |
| `bloom_intensity` | 0.55 |
| `sunshaft_color` | 0.62 |

## Mantido original

- `rain_intensity`, `rain_max_wetness` e `rain_additional_ambient`.
- `lightning_intensity` e máscaras de relâmpago.
- `diffuse`, `specular`, `env` e reflexos.
- Cores e texturas do céu, nuvens e sombras de nuvens.
- Frequência de mau tempo, molhamento e secagem em `climate.sii`.

