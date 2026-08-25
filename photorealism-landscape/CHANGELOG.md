# Changelog

## 0.7.0

- Identificador interno do manifest corrigido de `.photorealism_landscape`
  para `.pr_landscape`, respeitando o limite de 12 caracteres dos tokens SII.
- Nome, autor, categoria, descrição e ícone voltam a ser reconhecidos pelo
  Gerenciador de Mods.
- Validação automática do manifest adicionada ao empacotamento.
- Calibração gráfica consolidada da `0.6.0` preservada integralmente.

## 0.6.0

- Segundo passe de distância LOD das árvores.
- Árvores ampliadas de `330–390 m` para `420–480 m`.
- Faixa de transição de 60 metros preservada.
- Versão `0.5.0` registrada como checkpoint aprovado a 60 FPS e frame time
  estável.
- Vegetação de detalhe consolidada em `910–960 m` e demais parâmetros oficiais
  preservados.
- Versão consolidada após testes a 60 FPS e frame time estável.
- Manifest finalizado com autoria `Palamar`, categoria oficial `graphics` e
  ícone original do Photorealism Landscape em `276×162` pixels.

## 0.5.0-dev

- Primeiro passe de distância LOD das árvores.
- Árvores ampliadas dos valores oficiais `240–300 m` para `330–390 m`.
- Vegetação de detalhe consolidada em `910–960 m` e preservada.
- Pipeline ampliado para controlar árvores e grama separadamente.
- Terceiro componente dos vetores LOD e todos os demais parâmetros oficiais
  preservados.

## 0.4.0-dev

- Terceiro passe visual da vegetação de detalhe.
- Distância LOD ampliada de `800–850 m` para `910–960 m`.
- Alcance igualado ao mod de referência para comparação controlada.
- Faixa de transição de 50 metros preservada.
- Versão `0.3.0` preservada como checkpoint aprovado a 60 FPS, frame time
  estável e sem microstuttering.
- Árvores e todos os parâmetros não relacionados à vegetação preservados.

## 0.3.0-dev

- Segundo passe visual da vegetação de detalhe.
- Distância LOD ampliada de `650–700 m` para `800–850 m`.
- Faixa de transição de 50 metros preservada.
- Versão `0.2.0` registrada como checkpoint aprovado a 60 FPS e frame time
  estável.
- Árvores e todos os parâmetros não relacionados à vegetação preservados.

## 0.2.0-dev

- Primeira calibração visual da vegetação de detalhe.
- Distância LOD da grama ampliada de `410–460 m` para `650–700 m`.
- Faixa de transição de 50 metros preservada.
- Árvores, GPS, clima, chuva, iluminação e demais parâmetros globais
  preservados integralmente.
- Passe deliberadamente inferior aos `910–960 m` observados no Orion para
  avaliar primeiro o equilíbrio entre pop-in, FPS e estabilidade.

## 0.1.0-dev

- Estrutura inicial do Photorealism Landscape.
- Baselines oficiais de `game_data.sii` e `map_data.sii` do ETS2 1.60
  preservadas em `source/baseline/`.
- Pipeline reproduzível para calibrar somente a distância LOD da vegetação de
  detalhe em `game_data.sii`.
- Orion Landscape v1.2 estudado como referência técnica, sem incorporar seus
  assets, ícone ou metadados.
- Alterações de cores do GPS deliberadamente excluídas do módulo de paisagem.
- Pacote padronizado como
  `photorealism-landscape-<versão-do-mod>-<versão-do-jogo>.scs`.
