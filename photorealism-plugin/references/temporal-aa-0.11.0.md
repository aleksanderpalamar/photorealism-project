# AA temporal Photorealism 0.11.0

## Fronteira observavel

Nao existe API publica do Prism3D para substituir o TAA. Esta integracao e um
hook D3D11 nao oficial e defensivo. O recurso so e processado depois de ser
observado como render target e SRV consumido diretamente com o backbuffer
exato ligado em OM. A substituicao desse SRV ocorre antes dos draws posteriores
de GPS, textos, menus e interface.

Em resolucao nativa/supersampled, a trava exige R11G11B10, slot zero,
atividade recente, pelo menos doze usos diretos e doze confirmacoes. Familias
nao nativas novas exigem correlacao dimensional com o depth ativo. Esses criterios sao
evidencia de execucao, nao conhecimento do render graph proprietario.

## Algoritmo e limites

O compute shader executa filtragem espacial edge-aware, procura em uma janela
3x3 do historico a melhor correspondencia de cor/luma, limita o resultado ao
envelope 3x3 atual e reduz o peso quando movimento/rejeicao aumentam. Duas
texturas FP16 formam o historico ping-pong. RCAS de 0,4 stop faz sharpening
depois do temporal; EASU entra antes de RCAS somente em upscale seguro.

O plugin nao recebe jitter, matrizes anteriores nem motion vectors do motor.
Logo, a correspondencia e em espaco de tela e nao equivale a um TAA moderno
com reprojecao por vetor. A implementacao e temporal, mas nao sera chamada de
"melhor que o TAA nativo" antes de A/B em movimento e telemetria.

`Home` desativado impede a substituicao do SRV pre-interface e invalida o
historico. Isso preserva a comparacao vanilla e o contrato da captura F12 sem
criar uma tecla nova para AA/FSR.

## Gestao reversivel do config.cfg

O bootstrap reconhece somente `eurotrucks2.exe` e `amtrucks.exe`. No Documents
do jogo, preserva a primeira configuracao em
`config.photorealism-native-aa.backup.cfg` e grava o `config.cfg` por arquivo
temporario + `MoveFileExW` com replace/write-through. Altera somente:

- `r_aa` para `0`;
- `r_taa_tuning` para `0`;
- `r_taa_luma_sharpen` para `0.0`;
- `r_taa_modulated_drr_strength` para `0.0`, quando a chave existe.

`photorealism-aa-config.log` registra valores detectados/aplicados. Se o jogo
ja tiver lido o arquivo, a persistencia entra automaticamente na proxima
inicializacao. O plugin nunca religa o AA nativo por fallback.

Para restaurar o nativo, feche o jogo, remova/desative o `dinput8.dll` do
Photorealism e copie o backup sobre `config.cfg`. Manter o bootstrap instalado
desativaria novamente os valores na inicializacao seguinte.

## Teste real obrigatorio

Comparar ETS2/ATS em cabine e camera externa, vegetacao, cabos, placas, cidade,
noite e chuva; mover e girar a camera procurando ghosting, shimmer e halos.
Verificar GPS/textos/menus, resize, alt-tab e transicao menu/mundo. Confirmar no
log `AA Photorealism ativo`, `TemporalAA_avg/max`, estado EASU e uma unica
captura Steam por toque F12 com Home ativo e desativado.
