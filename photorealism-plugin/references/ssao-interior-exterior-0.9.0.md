# Refinamento SSAO interior e exterior 0.9.0

## Objetivo

Manter o SSAO exterior da 0.8.0, incluindo 16 amostras e protecao de altas
luzes, ao mesmo tempo em que a geometria proxima recebe uma oclusao mais
contida. O alvo visual e uma cabine com contato e volume natural, sem aspecto
sujo, halos escuros ou perda de detalhes no painel e nos retrovisores.

## Limitacao conhecida e criterio adotado

O depth buffer do ETS2 informa distancia e contorno, mas nao identifica a
semantica do objeto nem entrega uma flag de camera interna. Nao e seguro
classificar toda a imagem como "interior" usando apenas luminancia: uma cabine
ensolarada ou uma garagem clara seriam classificadas incorretamente.

Por isso, a 0.9.0 usa a distancia linear reversed-Z de cada pixel:

- `0` a `near_start=2.0m`: perfil interior total;
- `2.0m` a `near_end=8.0m`: mistura suave;
- alem de `8.0m`: perfil exterior da 0.8.0 integral.

Essa regra tambem beneficia objetos muito proximos vistos do exterior, onde
um raio curto reduz halos em pneus, grades, para-choques e guard-rails.

## Perfis

| Perfil | Raio | Intensidade | Bias | Rejeicao de bordas |
| --- | ---: | ---: | ---: | ---: |
| Exterior 0.8.0 | 0.80 | 0.28 | 0.04 | 1.50 |
| Interior 0.9.0 | 0.45 | 0.20 | 0.05 | 1.75 |

O maior bias e a maior rejeicao no perfil interior reduzem auto-oclusao e
vazamento de oclusao entre painel, vidros, colunas e retrovisores.

## Comparacao A/B

Para comparar somente esta camada, altere
`[module.ssao_interior.0.9.0] enabled=false` e pressione `End`. O SSAO 0.8.0,
o visual, a chuva e todas as demais camadas permanecem ativos.
