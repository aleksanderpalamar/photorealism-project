# Photorealism Lights

Módulo independente da coleção Photorealism dedicado às fontes de luz
artificial, lâmpadas e flares do Euro Truck Simulator 2.

## Estado atual

A versão `0.8.0` preserva integralmente as calibrações consolidadas e calibra a
projeção das luzes auxiliares nos 47 perfis oficiais. Os auxiliares dianteiros e
de neblina recebem 3% de alcance e 2% de intensidade difusa/especular. Os
auxiliares de teto e o estado dianteiro+teto recebem 4% de alcance e 3% de
intensidade. Temperatura de cor, largura, inclinação, máscaras, refração e a
hierarquia entre farol baixo, alto e auxiliares permanecem específicas de cada
caminhão.

O projeto separa deliberadamente iluminação artificial do Photorealism
Navigation. Dessa forma, o GPS continua sendo um módulo leve e independente.

## Estrutura

```text
photorealism-lights/
├── mod/
│   ├── def/
│   ├── unit/hookup/
│   ├── manifest.sii
│   ├── mod_description.txt
│   └── mod_icon.jpg
├── source/
│   ├── artwork/
│   ├── baseline/
│   ├── notes/
│   └── lights_profile.json
├── references/
├── tools/
│   ├── build_lights.py
│   ├── validate_manifest.py
│   └── package_mod.sh
└── dist/
```

## Cobertura planejada

- Flares visuais de postes brancos, neutros e quentes.
- Fluxo luminoso, alcance e distribuição dos postes.
- Flares, distância de visibilidade e cores dos semáforos.
- Flares de faróis, farol alto, lanternas, freios, setas, luzes de ré e
  auxiliares.
- Materiais emissivos associados às lâmpadas.
- Resposta visual de bloom de forma indireta e controlada.

O bloom global pertence ao renderizador. O `.scs` trabalhará sobre intensidade,
escala, alcance e materiais emissivos, que são os estímulos usados pelo bloom,
sem substituir shaders ou depender de ReShade.

## Geração e empacotamento

```sh
./tools/build_lights.py
./tools/package_mod.sh
```

O pacote é criado como:

```text
dist/photorealism-lights-0.8.0-1.60.scs
```

## Compatibilidade

Os arquivos em `unit/hookup` são globais. O mod poderá entrar em conflito com
outros pacotes de faróis, flares, semáforos ou iluminação urbana que substituam
os mesmos caminhos. Os módulos Photorealism, Photorealism Landscape e
Photorealism Navigation não usam esses arquivos e podem ser mantidos ativos.

O manifest usa a categoria oficial `graphics`, autoria `Palamar` e o
identificador SII válido `.pr_lights`.
