# Calibracao visual 0.2.0

Primeiro perfil desenhado para produzir uma diferenca perceptivel sem assumir
o aspecto de um filtro cinematografico agressivo.

| Controle | 0.1.2 | 0.2.0 | Intencao |
| --- | ---: | ---: | --- |
| temperatura | 6500 K | 6400 K | calor optico quase neutro |
| exposicao | -0.09 | -0.04 | preservar luminosidade util |
| contraste | 0.98 | 1.06 | profundidade fotografica moderada |
| saturacao | 0.95 | 0.98 | cor natural sem excesso |
| vibrance | -0.05 | 0.04 | recuperar cores discretas |
| sombras | 0.04 | 0.08 | leitura da cabine e vegetacao escura |
| realces | -0.05 | -0.14 | preservar ceu e superficies claras |
| pretos | -0.01 | -0.05 | ancora de preto mais firme |
| brancos | 0.03 | 0.04 | brilho limpo sem estourar |
| contraste local | 0.12 | 0.18 | textura e separacao de planos |
| nitidez | 0.18 | 0.22 | definicao perceptivel sem halos fortes |
| vinheta | 0.04 | 0.035 | enquadramento optico discreto |

## Cenas de validacao

- meio-dia seco, com ceu e veiculo branco;
- cabine voltada para uma area sombreada com exterior claro;
- chuva diurna, observando gotas, asfalto e reflexos;
- amanhecer ou entardecer, observando gradientes do ceu;
- noite urbana, verificando pretos, placas, farois e flares.

As comparacoes devem ser feitas com o caminhao parado e capturas externas antes
e depois de pressionar `Home`.
