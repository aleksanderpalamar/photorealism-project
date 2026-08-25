# Photorealism Landscape

Projeto separado do Photorealism principal para melhorar a distância visual e
a transição da vegetação no Euro Truck Simulator 2.

## Estado atual

A vegetação de detalhe foi consolidada na versão `0.4.0`, em `910–960 m`. O
primeiro passe das árvores, `0.5.0-dev` em `330–390 m`, foi aprovado com 60 FPS
e frame time estável. A calibração `0.6.0` consolidou as árvores em `420–480 m`.
A versão `0.7.0` corrige e valida os metadados do Gerenciador de Mods sem
alterar a calibração gráfica.

O Orion Landscape v1.2 foi estudado como referência técnica. A comparação
mostrou que seu efeito paisagístico vem apenas do aumento da distância LOD da
vegetação de detalhe/grama. As alterações de GPS presentes no mod de referência
ficaram fora deste projeto por não pertencerem ao escopo de paisagem.

## Estrutura

```text
photorealism-landscape/
├── mod/                         conteúdo que entra no arquivo .scs
│   ├── def/game_data.sii
│   ├── manifest.sii
│   ├── mod_icon.jpg
│   └── mod_description.txt
├── source/
│   ├── baseline/                definições oficiais do ETS2 1.60
│   ├── notes/                   plano e resultados de calibração
│   └── landscape_profile.json   perfil reproduzível
├── references/                  análise técnica de mods de referência
├── tools/
│   ├── build_landscape.py       gera game_data.sii a partir da baseline
│   ├── validate_manifest.py     valida metadados e limites dos tokens SII
│   └── package_mod.sh           cria o pacote .scs
└── dist/                        pacotes gerados
```

## Geração

O perfil fica em `source/landscape_profile.json`. Para reconstruir apenas a
definição ativa:

```sh
./tools/build_landscape.py
```

O script preserva todos os demais campos oficiais de `game_data.sii` e controla
separadamente o primeiro componente de `leaves_lod_start/end`, usado pelas
árvores, e o segundo componente, usado pela vegetação de detalhe.

## Empacotamento

```sh
./tools/package_mod.sh
```

O pacote padrão será criado como:

```text
dist/photorealism-landscape-0.7.0-1.60.scs
```

Para versões futuras:

```sh
MOD_VERSION=0.8.0 GAME_VERSION=1.60 ./tools/package_mod.sh
```

## Compatibilidade

`game_data.sii` é uma definição global e pode entrar em conflito com qualquer
outro mod que substitua o mesmo arquivo. O Photorealism principal não altera
esse arquivo, então os dois projetos podem ser usados juntos.

Nenhum asset, ícone ou texto do Orion Landscape foi incorporado ao pacote.

Os metadados usam a categoria oficial `graphics`, autoria `Palamar` e um ícone
original próprio em `276×162` pixels.
