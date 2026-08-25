# Core 0.10.2 - cadeia Present e captura Steam

## Hipotese observavel

As capturas F12 anteriores nao continham o passe que aparecia na tela. Isso e
compativel com o overlay Steam capturando o backbuffer antes do hook DXGI do
Photorealism. A ordem interna exata do overlay nao e documentada aqui e nao e
tratada como arquitetura confirmada do Steam ou do Prism3D.

## Alteracao defensiva

Depois que D3D11 aparece, o worker aguarda por no maximo tres segundos o
`gameoverlayrenderer64.dll`. Quatro amostras estaveis sao exigidas antes de
instalar. Sem o modulo, a instalacao segue pelo fallback nao-Steam.

O probe instala `Present` (slot 8), `Present1` (slot 22 quando DXGI 1.2 esta
disponivel), `ResizeBuffers` e os observers existentes. Um contador TLS cobre
toda a chamada downstream: apenas a entrada externa executa `process_frame`,
evitando duplicacao se `Present1` e `Present` se encadearem.

O log resolve o modulo proprietario do entry atual e do downstream no install,
na primeira chamada real e em auditorias apos 0,5, 2 e 5 segundos. O estado
`nosso-hook-externo` e o esperado; `substituido-ou-encadeado` indica que outro
componente alterou ou ficou por fora do entry observado.

## Limites e teste

Nao existe interceptacao de `VK_F12`, SteamAPI ou screenshot alternativa. A
compatibilidade so e aprovada se, no jogo real, Home ativo gerar uma captura
Steam com o efeito, Home desativado gerar vanilla, cada toque produzir uma
unica captura e ambas seguirem para o uploader normal. Se falhar, os enderecos
e owners registrados orientam a proxima correcao sem mover cegamente o passe.
