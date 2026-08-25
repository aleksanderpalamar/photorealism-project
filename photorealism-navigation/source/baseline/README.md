# Baselines oficiais — ETS2 1.60

Estes arquivos foram extraídos da instalação local oficial do Euro Truck
Simulator 2 versão 1.60 e são usados apenas como base reproduzível para gerar
as variantes do mod.

## `def/map_data.sii`

- SHA-256: `9433ef7c8d120509ee641f6d1643966ddc99effe1f237ec9c6b34025bdb72473`

## Scripts de interface

- `ui/adviser_gps.sii`:
  `f8a8d3467651d51abcf0d4ca140f94c5c14c757e6d43ab4acb8203099acf8dad`
- `ui/gps.sii`:
  `27b321372ed6849b0e90969ada83e62d9ce8a6ce639cbfaea09e7398c9465bae`
- `ui/dashboard/volvo_fh_2021_gps.sii`:
  `692ae3f9d9ff39437c765c4a76885ef90b8d90b58ba9132fd280db881a800a2e`
- `ui/dashboard/volvo_fh_2021_mph_gps.sii`:
  `dec562b9da25271e9d0e78a06b29c98d5403473c50ce4a1a7e86dcf62947c41a`
- `ui/dashboard/volvo_fh_2024_gps.sii`:
  `f9b121d29d50786ee65e762ef68aa72790b0a6502861a14aa9baecf71362a73c`
- `ui/dashboard/volvo_fh_2024_mph_gps.sii`:
  `fb1f787baadf2fe9804c91c509f05dae7d6f5ca5a887a46bc4f41b1befdf90f1`

O gerador nunca modifica essas baselines. Cada construção começa novamente a
partir delas e escreve o resultado em `build/light` ou `build/dark`.
