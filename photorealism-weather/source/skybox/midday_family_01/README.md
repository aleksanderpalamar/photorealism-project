# Família diurna 01

Variações do skybox `clear_midday_02` para suavizar as transições solares entre
o meio-dia e o início da tarde.

## Perfis

- `nice.16`: aproximação do meio-dia, levemente mais fria.
- `nice.17`: composição aprovada na versão 0.6.1.
- `nice.18`: ápice solar, um pouco mais claro e azul.
- `nice.19`: primeira descida, discretamente mais quente.
- `nice.20`: tarde inicial, mais suave e quente.
- `nice.21`: saída gradual da família antes dos perfis de menor elevação.

Todos preservam exatamente a posição e a escala das nuvens. A mesma máscara
`clear_midday_02-mask` é compartilhada pelos seis perfis.

## Validação

- Panoramas: 4096×1024, `R9G9B9E5_SHAREDEXP`, 13 mipmaps.
- Máscara: 2048×512, `R8_UNORM`, 12 mipmaps.
- Continuidade horizontal de cada fonte: RMSE igual a zero.
- Conversão concluída sem erros com SCS Conversion Tools 2.21.
