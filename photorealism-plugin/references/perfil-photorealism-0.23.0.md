# Perfil de tom 0.23.0

Estado: entregue e aprovado no jogo pelo usuario. Continua em `perfil-photorealism-0.23.1.md`.

## De onde vem os valores

O usuario forneceu um cfg de referencia, no formato de outro plugin de iluminacao, e pediu que o
photorealism-plugin tenha aquele efeito. A implementacao e independente: as formulas do plugin de
origem nao sao conhecidas e nao foram obtidas do binario dele. Os valores do cfg entram nos controles
homonimos do grade do photorealism-plugin. **Mesmos valores nao garantem a mesma imagem.**

## Qual conjunto

O cfg de referencia tem cinco conjuntos de tom e um seletor de metodo de iluminacao
(`LightingMethod=3`, contando do zero). Tres indicios mostram que os conjuntos sao um por metodo, e
que so o do metodo ativo vale:

1. o changelog publico do plugin de origem registra que a pagina de tom do quarto metodo (iluminacao
   padrao do jogo) foi acrescentada depois;
2. o conjunto 4 e o unico sem pre-exposicao, pre-contraste e contraste dinamico -- a pagina mais simples;
3. em dois cfgs do usuario, com o seletor em 2 e em 3, o conjunto ajustado e o de numero seletor + 1, e
   os conjuntos 1 e 2 sao identicos nos dois, sobras que nenhum usa.

O photorealism-plugin nao tem metodos de iluminacao; a escolha vira `tonemap_set=4`. O quarto metodo e a
iluminacao padrao do jogo, entao seguir este cfg nao exige trocar a iluminacao do ETS2.

## Como cada valor entra

| cfg de referencia | photorealism-plugin | Observacao |
|---|---|---|
| Temp 6500 | `tonemap_temperature_4` -> temperatura | Kelvin, fixo: com o perfil a cor nao adapta por condicao |
| Exposure -0.06 | exposicao | EV (`exp2`) |
| Contrast 0.99, Saturation 1.00 | contraste, saturacao | 1 e neutro nos dois |
| Vibrance 0, Shadows -0.01, Highlights -0.07 | vibracao, sombras, altas luzes | sombras e altas luzes em EV por mascara |
| Blacks 0, Whites -0.01 | pretos, brancos | aditivos |
| NightExp 0 | lido, nao aplicado | zero no conjunto 4 |
| sharpness 6 | nitidez 0.6 | escala 0-10 dividida por 10: **suposicao** |
| sharpenedges 4 | contraste local 0.4 | e o termo de nitidez ponderado por borda do grade: **suposicao** |
| SSAO_Intensity 1.5 | x1.5 nas duas intensidades de SSAO | aplicado no desenho, nao gravado |

Campos do grade que o cfg nao tem ficam neutros no perfil: vinheta 0, matiz 0, joelho de altas luzes 0,
piso de preto 0. As camadas medidas (`base.0.1.2`, `visual.0.2.0`, `rain_overcast.0.3.0`) ficam no cfg,
intactas, fora da composicao; desligar o perfil devolve exatamente o grade medido.

## O que fica para as proximas versoes

- FXAA (`fxaa2=1`);
- descoberta do G-buffer e do HDR do jogo, e depois os efeitos entre passes: saturacao de albedo,
  iluminacao de interior, espelhos, normais da estrada, SSS, motion blur;
- vegetacao e chuva exigem troca de shader e dependem de nova decisao do usuario;
- pre-exposicao, pre-contraste, contraste dinamico e exposicao noturna: lidos e registrados no log
  quando fora do neutro, ainda nao aplicados;
- sem implementacao: operador de tom (`Tonemap_Tonemap`, `Tonemap_TonemapA`, significado desconhecido),
  AA/DLSS e presets de qualidade proprios do plugin de origem.
