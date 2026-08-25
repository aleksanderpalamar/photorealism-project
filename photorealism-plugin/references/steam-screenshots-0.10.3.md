# Core 0.10.3 - captura F12 oficial do Steam

O core nao consulta nem intercepta `VK_F12`. Quando `steam_api64.dll` e
`ISteamScreenshots` v003 estao disponiveis, registra o callback oficial
`ScreenshotRequested_t` (ID 2302) e somente entao chama
`HookScreenshots(true)`. Assim, o overlay deixa de gravar em paralelo.

Cada callback incrementa uma fila limitada. Depois de todos os passes
Photorealism no `Present`, a fronteira explicita `observe_postprocessed_frame`
recebe o swap chain e o render thread copia o backbuffer para uma textura
staging e encerra uma query D3D11. Em frames seguintes, `Map` ocorre somente
quando a query esta pronta. Um unico worker, com dois slots prealocados,
converte RGBA/BGRA em RGB. `WriteScreenshot` permanece no render thread e e
chamado exatamente uma vez para cada callback. O handle segue para a biblioteca
e o uploader normais do Steam.

Se Steamworks nao carregar, a interface/export estiver ausente ou o worker nao
iniciar, `HookScreenshots` nunca e ativado. Se formato/readback/WriteScreenshot
falhar depois de assumir a captura, o core chama `HookScreenshots(false)`,
remove o callback e usa `TriggerScreenshot` apenas para recuperar a solicitacao
sem handle. Resize e troca de device param o worker por evento, fazem join,
fecham handles e liberam staging/query antes de reinicializar.

Teste obrigatorio no Proton/DXVK:

1. Home ativo, um toque F12: uma captura com os efeitos;
2. Home desativado, um toque F12: uma captura vanilla;
3. ambas aparecem no uploader/gerenciador Steam;
4. resize/alt-tab e repetir;
5. conferir no log ativacao v003 ou fallback explicito.
