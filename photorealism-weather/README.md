# Photorealism

Projeto de desenvolvimento de um mod gráfico de clima e iluminação para o jogo Euro Truck Simulator 2.

## Estrutura

- `mod/`: conteúdo que será carregado pelo jogo e empacotado como `.scs`.
- `references/`: imagens de referência organizadas por condição de luz.
- `source/`: texturas editáveis, capturas, medições e anotações.
- `tools/`: utilitários locais; não entra no pacote do mod.
- `dist/`: pacotes gerados; não deve ser editado manualmente.

## Baseline

O pacote começa com cópias inalteradas dos perfis oficiais:

- `mod/def/climate/default/nice.sii`
- `mod/def/climate/default/bad.sii`

As cópias originais também ficam preservadas em `source/baseline/default/`. O arquivo
`def/climate.sii` não faz parte desta primeira versão.

## Calibração ativa

Os passes ativos cobrem o ciclo completo de 24 horas nos 34 perfis de `nice.sii`
e `bad.sii`, além da dinâmica das gotas no para-brisa, do spray das rodas e do
comportamento regional do clima e da pista. Os valores estão documentados em
`source/notes/midday-pass-01.md`, `source/notes/golden-hours-pass-01.md` e
`source/notes/bad-weather-pass-01.md`. A calibração noturna está em
`source/notes/night-pass-01.md`.
O primeiro passe de física de chuva está em `source/notes/orion-rain-study-01.md`.
Frequência, molhamento e secagem estão em
`source/notes/climate-behavior-pass-01.md`.

Para reconstruir todos os perfis calibrados a partir das baselines:

```sh
./tools/build_climate.py
```

## Ordem de trabalho sugerida

1. Calibrar meio-dia em `nice.sii`.
2. Calibrar amanhecer e entardecer.
3. Calibrar noite.
4. Repetir o processo em `bad.sii`.
5. Validar a família de skyboxes próprios entre o meio-dia e o início da tarde.
6. Ajustar frequência de clima, molhamento e secagem após a aparência estar estável.

Altere um grupo por vez: iluminação, atmosfera, exposição, pós-processamento e,
por último, texturas.

## Teste no Linux

Durante o desenvolvimento, copie a pasta `mod/` para uma pasta com nome próprio em:

```text
~/.local/share/Euro Truck Simulator 2/mod/photorealism/
```

O conteúdo precisa ficar diretamente na raiz dessa pasta: `manifest.sii`, `def/`
e `asset/`. Ative o mod acima de outros mods de clima ou gráficos.

Confira `game.log.txt` após cada teste e procure por `ERROR` e `WARNING`.

## Empacotamento

Execute:

```sh
./tools/package_mod.sh
```

Por padrão, o pacote será criado em
`dist/photorealism-0.11.0-1.60.scs`, seguindo o formato
`photorealism-<versão-do-mod>-<versão-do-jogo>.scs`.

Para informar outras versões sem editar o script:

```sh
MOD_VERSION=0.12.0 GAME_VERSION=1.60 ./tools/package_mod.sh
```
