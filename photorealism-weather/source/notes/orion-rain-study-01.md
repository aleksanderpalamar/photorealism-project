# Estudo Orion Elite Rains — passe 0.9.0

## Conteúdo observado

O pacote de referência altera quatro áreas: configuração das gotas no vidro,
material e textura da chuva, partículas de spray das rodas e máscaras laterais
específicas de diversas cabines. Também inclui veículos exclusivos do ATS, que
não pertencem ao escopo do Photorealism para ETS2.

## Comparação com o jogo-base 1.60

O jogo-base usa 5.000 gotas potenciais por metro, vida entre 1 e 8 segundos,
efeito de velocidade entre 10 e 70 km/h e parâmetros moderados de gravidade e
aderência. O Orion utiliza 15.000 gotas, vida entre 0,5 e 5 segundos, resposta
aerodinâmica entre 25 e 108 km/h e aderência muito elevada.

## Estratégia própria

O Photorealism adota um ponto intermediário de 9.000 gotas, tamanhos entre
0,007 e 0,021, vida entre 0,8 e 6,5 segundos e resposta de velocidade entre
15 e 90 km/h. A deformação, aderência e aceleração foram escolhidas para manter
movimento visível sem transformar o para-brisa em uma camada uniforme de água.

O spray usa geradores originais de baixa opacidade e cresce progressivamente em
tamanho, duração e quantidade. Os modelos compilados referenciados pelo Orion
não estavam presentes no `.scs` e continuam sendo fornecidos pelo jogo-base.
Os níveis específicos de asfalto combinam uma camada de névoa com uma segunda
camada esparsa de gotas, usando os sprites de água incorporados.

Com autorização do usuário, as texturas e materiais de chuva/spray do pacote de
referência foram incorporados para teste privado, além das máscaras das cabines
oficiais europeias. O conteúdo exclusivo de caminhões do ATS foi descartado.

## Fora do primeiro passe

- Máscaras e qualquer conteúdo exclusivo do ATS.
- Modelos compilados ausentes do arquivo de referência.
- Alterações na intensidade, frequência ou molhamento climático.
- Neve, sons de chuva e física de dirigibilidade.

## Autoria e redistribuição

Os assets incorporados são atribuídos ao Orion, autor de Elite Rain & Spray
Physics. O pacote analisado não continha licença explícita de redistribuição.
Antes de publicar uma versão do Photorealism contendo esses assets, é necessário
obter autorização do autor ou substituí-los por assets próprios.
