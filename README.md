# Photorealism Project

Coleção de mods e ferramentas gráficas do Palamar para Euro Truck Simulator 2
e American Truck Simulator.

## Componentes

- `photorealism-weather`: clima, céu, chuva e iluminação;
- `photorealism-landscape`: calibração visual de vegetação e paisagem;
- `photorealism-lights`: flares, iluminação urbana e luzes dos veículos;
- `photorealism-navigation`: variantes Light e Dark da navegação;
- `photorealism-plugin`: plugin gráfico Direct3D 11 para execução via Proton
  ou Windows.

Cada componente possui seu próprio `README.md`, código-fonte, arquivos de
construção e validação. Saídas geradas localmente — como `build/`, `dist/`,
caches, binários, logs e arquivos de depuração — não fazem parte do controle
de versão.

## Uso do repositório

Os scripts de cada componente geram os pacotes de distribuição localmente.
Consulte o README do componente correspondente antes de construir ou instalar
um mod.

## Terceiros

O plugin inclui o FidelityFX FSR 1 da AMD sob a licença MIT, mantida em
`photorealism-plugin/third_party/fidelityfx-fsr/LICENSE.txt`.
