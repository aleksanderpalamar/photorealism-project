# Projeto Photorealism

[Read in English](README.md)

Coleção de mods e ferramentas gráficas do Palamar. Os mods `.scs` desta
coleção são destinados ao Euro Truck Simulator 2; o Photorealism Plugin é o
componente gráfico compatível com Euro Truck Simulator 2 e American Truck
Simulator.

## Componentes

- `photorealism-weather` (ETS2): clima, céu, chuva e iluminação;
- `photorealism-landscape` (ETS2): calibração visual de vegetação e paisagem;
- `photorealism-lights` (ETS2): flares, iluminação urbana e luzes dos
  veículos;
- `photorealism-navigation` (ETS2): variantes Light e Dark da navegação;
- `photorealism-plugin` (ETS2/ATS): plugin gráfico Direct3D 11 para execução
  via Proton ou Windows.

Cada componente possui seu próprio README, código-fonte, arquivos de
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

## Licença

Os materiais originais do projeto são disponibilizados sob a
[Photorealism Personal Use License — All Rights Reserved](LICENSE). Ela
permite somente uso pessoal e não comercial; os direitos de venda,
sublicenciamento, distribuição comercial e inclusão em packs pagos são
reservados exclusivamente a Palamar. Componentes de terceiros permanecem sob
suas próprias licenças.

A tradução em português da licença está em [LICENSE.pt-BR.md](LICENSE.pt-BR.md)
somente para conveniência. Em caso de divergência, prevalece o texto inglês de
`LICENSE`.
