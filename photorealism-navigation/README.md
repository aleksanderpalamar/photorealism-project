# Photorealism Navigation

Módulo independente da coleção Photorealism dedicado à legibilidade e à
identidade visual do GPS e dos mapas do Euro Truck Simulator 2.

## Estado atual

A versão `0.2.0` é a primeira calibração visual. Ela oferece duas variantes
mutuamente exclusivas, `Light` e `Dark`, inspiradas na clareza dos aplicativos
de mapas modernos, sem usar marca, logotipo ou assets do Google Maps.

As duas variantes usam rota azul, hierarquia visual própria e fundos
calibrados para o Route Advisor, o GPS comum e os GPS Volvo FH 2021/FH 2024.
O `map_data.sii` também aplica a paleta correspondente no mapa mundial e nos
mapas de seleção de trabalho.

O Orion Landscape v1.2 foi estudado como referência técnica. Apesar da
descrição mencionar GPS em alta definição e modo noturno, o pacote não contém
texturas HD e não altera o brilho noturno. Suas mudanças reais são cores e
opacidade em `map_data.sii`.

## Estrutura

```text
photorealism-navigation/
├── mod/
│   ├── def/map_data.sii
│   ├── manifest.sii
│   ├── mod_description.txt
│   └── mod_icon.jpg
├── source/
│   ├── artwork/
│   ├── baseline/
│   │   ├── map_data.sii
│   │   └── ui/
│   ├── notes/
│   └── profiles/
│       ├── light.json
│       └── dark.json
├── references/
├── tools/
│   ├── build_navigation.py
│   ├── validate_manifest.py
│   └── package_mod.sh
├── build/
└── dist/
```

## Geração

Os perfis ficam em `source/profiles/light.json` e
`source/profiles/dark.json`:

```sh
./tools/build_navigation.py --theme all
```

O script parte sempre das definições oficiais do ETS2 1.60 e gera as variantes
em `build/light` e `build/dark`. Também é possível gerar apenas uma delas com
`--theme light` ou `--theme dark`.

## Empacotamento

```sh
./tools/package_mod.sh
```

Os pacotes serão criados como:

```text
dist/photorealism-navigation-light-0.2.0-1.60.scs
dist/photorealism-navigation-dark-0.2.0-1.60.scs
```

Ative somente um deles por vez no Gerenciador de Mods.

## Compatibilidade

`map_data.sii` e os scripts de GPS são definições globais. O mod pode entrar em
conflito com mods de mapa, GPS, Route Advisor ou interface que substituam os
mesmos arquivos. O Photorealism e o Photorealism Landscape não alteram esses
arquivos e podem ser usados junto.

O ETS2 1.60 não expõe no `map_data.sii` uma segunda paleta associada ao horário.
Por isso, a troca Light/Dark é feita escolhendo a variante no Gerenciador de
Mods, e não automaticamente durante a viagem.

Em mapas expandidos, uma definição específica pode ser necessária para
preservar limites de câmera e enquadramento do mapa mundial.

Nenhum asset, ícone, descrição ou arquivo do Orion foi incorporado ao pacote.
