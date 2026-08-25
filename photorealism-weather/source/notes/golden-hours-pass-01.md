# Amanhecer e entardecer — passe 01

Escopo:

- Amanhecer: `default.nice.05` até `default.nice.13`.
- Entardecer: `default.nice.23` até `default.nice.31`.

O intervalo cobre a transição entre aproximadamente -15° e +25° de elevação
solar, separando manhã (`sun_direction: 1`) e tarde (`sun_direction: -1`).

## Amanhecer

| Campo | Multiplicador |
|---|---:|
| `ambient` | 0.96 |
| `sun_shadow_strength` | 0.90 |
| `fog_density` | 1.15 |
| `color_saturation` | 0.97 |
| `contrast` | 0.94 |
| `bloom_threshold` | 1.20 |
| `bloom_limit` | 0.90 |
| `bloom_intensity` | 0.65 |
| `sunshaft_color` | 0.72 |

A densidade atmosférica recebe o maior reforço neste período para favorecer
profundidade, névoa matinal e uma transição mais natural no horizonte.

## Entardecer

| Campo | Multiplicador |
|---|---:|
| `ambient` | 0.95 |
| `sun_shadow_strength` | 0.90 |
| `fog_density` | 1.08 |
| `color_saturation` | 0.96 |
| `contrast` | 0.93 |
| `bloom_threshold` | 1.20 |
| `bloom_limit` | 0.88 |
| `bloom_intensity` | 0.62 |
| `sunshaft_color` | 0.68 |

O entardecer mantém a atmosfera original, mas segura contraste, bloom e raios
solares para evitar o aspecto excessivamente laranja ou luminoso.

## Mantido original

- `diffuse`, `specular`, `env` e `env_static_mod`.
- `sun_color`, `sky_color`, `fog_color` e `fog_color2`.
- Texturas e máscaras de céu.
- Exposição automática e `target_gray`.

