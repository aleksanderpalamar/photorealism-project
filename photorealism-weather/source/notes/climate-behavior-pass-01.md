# Frequência climática e molhamento — passe 01

Escopo: `/def/climate.sii`, somente nos perfis regionais `default`, `cold`,
`arid` e `desert`.

O objetivo é deixar a mudança natural do clima mais presente sem provocar
chuva constante, fazer o asfalto acumular água progressivamente e manter a
pista molhada por um período plausível depois que a chuva termina.

## Calibração

| Perfil | Mau tempo (base → mod) | Molhamento (base → mod) | Secagem (base → mod) |
|---|---:|---:|---:|
| `default` | `0.07 → 0.08` | `0.10 → 0.11` | `0.010 → 0.0075` |
| `cold` | `0.07 → 0.09` | `0.10 → 0.115` | `0.010 → 0.0065` |
| `arid` | `0.03 → 0.03` | `0.10 → 0.10` | `0.010 → 0.012` |
| `desert` | `0.01 → 0.01` | `0.10 → 0.09` | `0.010 → 0.016` |

## Intenção por região

- `default`: chuva discretamente mais presente e pista secando mais devagar.
- `cold`: maior persistência da água e um pouco mais de mau tempo.
- `arid`: frequência preservada e secagem moderadamente mais rápida.
- `desert`: chuva continua rara e a pista perde água com maior rapidez.

`drying_factor` menor significa perda mais lenta da água acumulada; valores
maiores aceleram a secagem. O passe evita extremos para que as transições ainda
possam ser avaliadas durante uma entrega normal.

## Preservado

- Os perfis técnicos `reference`, `albedo`, `black`, `integrity` e `grayscale`.
- Os pesos de skybox e as diferenças regionais oficiais.
- `nice.sii`, `bad.sii`, intensidade da chuva e relâmpagos.
- Gotas no vidro, limpadores, materiais, máscaras e spray da versão 0.9.0.
- Iluminação, exposição, atmosfera e skyboxes já aprovados.
