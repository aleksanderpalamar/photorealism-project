# Noite — passe 01

Escopo noturno em tempo bom e chuvoso:

- `default.nice.01`–`.04` e `.32`–`.34`.
- `default.bad.01`–`.04` e `.32`–`.34`.

Os dois intervalos representam a noite antes do nascer do sol e depois do pôr do
sol. Os mesmos multiplicadores são usados nas duas extremidades para evitar uma
diferença arbitrária entre madrugada e começo da noite.

## Tempo bom

| Campo | Multiplicador | Intenção |
|---|---:|---|
| `fog_density` | 1.04 | Acrescentar profundidade sem encurtar muito a visão. |
| `color_saturation` | 0.96 | Conter iluminação urbana excessivamente colorida. |
| `contrast` | 0.98 | Preservar leitura das sombras e da cabine. |
| `bloom_threshold` | 1.35 | Exigir uma fonte mais luminosa para produzir halo. |
| `bloom_limit` | 0.82 | Limitar picos ao redor de faróis e postes. |
| `bloom_intensity` | 0.55 | Reduzir o véu luminoso. |
| `bloom_standard_deviation` | 0.90 | Tornar o halo um pouco mais estreito. |
| `sunshaft_color` | 0.65 | Conter shafts noturnos. |

## Mau tempo

| Campo | Multiplicador | Intenção |
|---|---:|---|
| `fog_density` | 1.08 | Reforçar a dispersão atmosférica da chuva. |
| `color_saturation` | 0.95 | Conter cores refletidas no asfalto molhado. |
| `contrast` | 0.98 | Manter gotas e spray definidos. |
| `bloom_threshold` | 1.40 | Restringir halos sob chuva. |
| `bloom_limit` | 0.80 | Limitar picos luminosos. |
| `bloom_intensity` | 0.50 | Reduzir bloom espalhado pela chuva. |
| `bloom_standard_deviation` | 0.88 | Estreitar os halos. |
| `sunshaft_color` | 0.60 | Conter shafts em céu encoberto. |

## Mantido original

- `target_gray`, `min_scale`, `max_scale` e `scale_override`.
- Velocidades de adaptação ao claro e ao escuro.
- `ambient`, `diffuse`, `specular`, `env` e `env_static_mod`.
- Potência e cor dos faróis, painel e luzes urbanas.
- Lua, estrelas e texturas do céu.
- Chuva, molhamento, spray e relâmpagos.

