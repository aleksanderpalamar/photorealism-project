# Changelog

## 0.2.0

- Primeira calibração visual do Photorealism Navigation.
- Variantes `Light` e `Dark` geradas como pacotes independentes e mutuamente
  exclusivos.
- Paleta azul inspirada na legibilidade dos mapas modernos, com cores autorais
  e sem assets ou marca do Google Maps.
- Fundos dedicados para Route Advisor, GPS comum e GPS dos Volvo FH 2021/FH
  2024, incluindo as variantes MPH.
- Cores de estradas, prefabs, áreas de mapa, contorno, rota, destaque, segmento
  percorrido e setas calibradas em `0xFFBBGGRR`.
- Pipeline atualizado para gerar, validar e empacotar as duas variantes de uma
  só vez.
- Pacotes padronizados como
  `photorealism-navigation-<tema>-0.2.0-1.60.scs`.

## 0.1.0

- Estrutura inicial do Photorealism Navigation.
- Baseline oficial de `map_data.sii` do ETS2 1.60 preservada.
- Pipeline reproduzível para calibração isolada das cores do GPS e dos mapas.
- Orion Landscape v1.2 estudado como referência técnica, sem copiar seus assets
  ou metadados.
- Manifest com categoria oficial `ui`, autoria `Palamar` e identificador SII
  válido `.pr_nav`.
- Ícone original criado no padrão visual da coleção Photorealism.
- Pacote padronizado como
  `photorealism-navigation-<versão-do-mod>-<versão-do-jogo>.scs`.
