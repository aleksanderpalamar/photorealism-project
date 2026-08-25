# Céu claro de meio-dia 02

Revisão visual do primeiro skybox, criada para a versão 0.6.1.

## Correção geométrica

A fonte foi gerada como panorama equiretangular esférico 2:1, com o horizonte
exatamente no centro. Somente o hemisfério superior foi recortado, produzindo
o panorama 4:1 usado pelo ETS2 sem alongar as nuvens horizontalmente.

## Arquivos

- `generated/clear_midday_02-fullsphere-master.png`: fonte esférica 2:1.
- `clear_midday_02-4096x1024.png`: hemisfério superior em 4:1.
- `clear_midday_02-mask-2048x512.png`: máscara alinhada de nuvens e névoa.
- `mid/`: HDR, TGA e TOBJ textuais usados na conversão.

## Especificação

- Cor: 4096×1024, DDS `R9G9B9E5_SHAREDEXP`, 13 mipmaps.
- Máscara: 2048×512, DDS `R8_UNORM`, 12 mipmaps.
- Pico HDR das nuvens: aproximadamente 1,5, reduzido em relação à versão 0.6.0.
- Continuidade horizontal do panorama e da máscara: RMSE zero.
- Uso limitado ao perfil `default.nice.17` durante a validação.
