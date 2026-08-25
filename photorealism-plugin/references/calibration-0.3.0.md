# Calibracao visual 0.3.0

Perfil unico equilibrado para cenas secas, chuva e tempo nublado. A versao nao
tenta detectar o clima automaticamente; os controles foram ajustados para
funcionar de maneira segura nos tres cenarios.

## Alteracoes sobre a 0.2.0

| Controle | 0.2.0 | 0.3.0 | Intencao |
| --- | ---: | ---: | --- |
| exposicao | -0.04 | -0.03 | preservar leitura em tempo fechado |
| contraste | 1.06 | 1.07 | separar planos sob luz difusa |
| saturacao | 0.98 | 0.97 | evitar cores artificiais sob chuva |
| vibrance | 0.04 | 0.05 | manter cores discretas presentes |
| sombras | 0.08 | 0.10 | leitura da cabine e vegetacao escura |
| realces | -0.14 | -0.18 | preservar nuvens e ceu cinzento |
| pretos | -0.05 | -0.06 | manter profundidade depois de abrir sombras |
| brancos | 0.04 | 0.08 | valorizar reflexos molhados e sinalizacao |
| contraste local | 0.18 | 0.24 | textura em asfalto e fachadas |
| nitidez | 0.22 | 0.20 | reduzir dureza em gotas e bordas finas |
| vinheta | 0.035 | 0.03 | preservar luminosidade periferica |

## Clareza seletiva

O shader agora limita a parcela de contraste local mais forte aos medios tons.
A nitidez basica continua presente em toda a imagem, enquanto sombras muito
escuras e realces intensos recebem menos reforco. Isso reduz ruido na cabine,
halos no ceu e contornos excessivos nas gotas de chuva.

## Teste recomendado

- capturas A/B paradas no mesmo patio durante chuva;
- asfalto sob luz difusa e sob reflexo direto;
- gotas no para-brisa com limpador parado e em movimento;
- ceu nublado, fachada clara e cabine na mesma composicao;
- uma cena seca ao meio-dia para garantir que o perfil geral foi preservado.
