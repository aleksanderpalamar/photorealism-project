# Céu claro de meio-dia 01

Primeiro skybox próprio do projeto. A versão de teste está limitada ao perfil
`default.nice.17`; os 13 slots desse perfil usam o mesmo panorama e a mesma
máscara para eliminar variação aleatória durante a avaliação.

## Arquivos

- `generated/clear_midday_01-master.png`: resultado original da geração.
- `clear_midday_01-4096x1024.png`: panorama ajustado à projeção 4:1.
- `clear_midday_01-mask-2048x512.png`: máscara derivada de nuvens e névoa.
- `mid/`: fontes HDR/TGA e TOBJ textuais usados pelo conversor da SCS.
- `mod/asset/skybox/photorealism/`: DDS e TOBJ binários carregados pelo jogo.

## Especificação validada

- Cor: 4096×1024, DDS `R9G9B9E5_SHAREDEXP`, 13 mipmaps.
- Máscara: 2048×512, DDS `R8_UNORM`, 12 mipmaps.
- Endereçamento: repetição horizontal e clamp vertical.
- Continuidade: diferença RMSE entre primeira e última coluna igual a zero.

O panorama HDR foi balanceado contra um skybox diurno oficial antes da
conversão. A fonte colorida preserva picos próximos de 1,8 nas nuvens e médias
RGB próximas às do panorama oficial usado como referência.

## Estado

Convertido sem erros com SCS Conversion Tools 2.21 via Wine. Ainda é uma
variação experimental: não reutilize este céu no amanhecer, entardecer, noite
ou mau tempo antes do teste visual dentro do jogo.
