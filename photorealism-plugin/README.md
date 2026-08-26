# Photorealism Plugin

Plugin grafico independente para Euro Truck Simulator 2 e American Truck
Simulator, voltado ao caminho Windows x64 / Direct3D 11 executado pelo Proton
e traduzido para Vulkan pelo DXVK.

Autor: Palamar

O shader e compilado em tempo de execucao por `d3dcompiler_47.dll`, componente
normalmente fornecido pelo ambiente Proton.

## Estado da versao 0.11.3 + Photorealism FSR/AA 0.7.0

A 0.11.3/0.7.0 e a etapa diagnostica segura da integracao de AA/FSR. Ela
observa `Draw`, `DrawIndexed`, `DrawInstanced` e `DrawIndexedInstanced`, mas
so conta uma prova quando o estado vivo do D3D11 confirma uma composicao final
para o backbuffer. A mesma assinatura precisa ocorrer em 24 frames para ser
bloqueada. Esta entrega nao substitui SRVs e nao executa EASU, Temporal ou
RCAS: o log deve registrar `replacement=0 dispatch=0`. Assim, a imagem desta
versao deve permanecer igual a 0.11.2 enquanto coleta a prova que permitira
ativar Temporal + RCAS automaticamente e com seguranca na etapa seguinte.

## Estado anterior: 0.11.2 + Photorealism FSR/AA 0.6.1

A 0.11.2/0.6.1 corrige uma substituicao insegura de recursos que podia causar
uma imagem repetida em quadrantes. O modulo FSR/AA agora falha fechado: ele
mantem a observacao diagnostica, mas nao troca nenhum scene-SRV antes de provar
o draw final. Assim, EASU, RCAS e AA temporal auxiliares ficam em pass-through
temporariamente. O pipeline visual principal do `dxgi.dll` continua ativo,
incluindo as calibracoes de iluminacao, SSAO e resolve temporal ja consolidadas.

## Estado historico: 0.11.0 + Photorealism FSR/AA 0.6.0

Esta versao preserva byte a byte a configuracao e os quatro shaders visuais
consolidados e acrescenta AA espacial/temporal proprio antes da interface,
alem do FSR 1 espacial real. A instalacao contem
duas DLLs de integracao e um modulo auxiliar:

- `dinput8.dll`: bootstrap leve e proxy do DirectInput;
- `dxgi.dll`: proxy DXGI e nucleo grafico que controla hook, shader,
  configuracao e log.
- `photorealism-fsr.dll`: modulo auxiliar opcional, carregado explicitamente
  pelo nucleo; nunca atua como proxy.

O Photorealism FSR 0.1.0 consolidou a infraestrutura, a API C/ABI e o
diagnostico do dispositivo D3D11 real. A 0.2.0 ampliou essa fronteira com uma
ABI v2 retrocompativel e observa os render targets de cor vinculados pelo jogo.
Ela cataloga texturas reais por identidade do recurso e registra resolucao,
formato, MSAA, flags, views, slots, frequencia e ordem de atividade em janelas
de 30 segundos.

Chamadas produzidas pelo proprio passe Photorealism nao sao encaminhadas. O
hot path usa caches fixos; consultas COM ocorrem somente quando uma RTV ainda
nao existe no cache da janela. Ao terminar cada janela, `Present` copia apenas
um snapshot limitado para uma fila fixa de dois slots. Um unico worker faz a
ordenacao e toda a escrita do relatorio; nao ha I/O de disco na thread de
`Present`. Saturacao da fila e contabilizada no relatorio, sem criar threads ou
filas ilimitadas. Os relatorios usam rotulos conservadores por evidencia:
apresentacao confirmada pela identidade do backbuffer e candidatos provaveis
de cena, espelho/reflexo ou interface. Esses rotulos nao confirmam a semantica
interna proprietaria do Prism3D.

A 0.6.0 mantem a ABI v4 e observa o consumo de SRVs no pixel shader.
Uma textura so pode ser promovida quando foi vista como render target, e
depois consumida repetidamente como entrada enquanto o backbuffer exato esta
ligado como saida. Essa relacao e evidencia observavel do passe de composicao;
nao e apresentada como conhecimento privado do render graph Prism3D.

O modulo aceita dinamicamente R16F, R11G11B10 e formatos UNORM/sRGB. EASU
exige fonte menor nos dois eixos, escala 1,05x-2,00x, erro de proporcao de no
maximo 1,5%, sample/mip/array unitarios, atividade recente, correlacao com o
depth ativo e doze confirmacoes. Em resolucao nativa ou supersampling, o AA
proprio exige R11, slot zero e prova forte de composicao; resolucao nao nativa
nova exige correlacao com depth. R16F nunca e forcado.

Antes de eventual upscale, o modulo executa AA espacial edge-aware e resolve
temporal com historico ping-pong, clamp de vizinhanca, rejeicao de cor e busca
local 3x3 de correspondencia. Nao ha jitter nem vetores de movimento do
Prism3D: e uma reconstrucao temporal defensiva, nao reprojecao motion-vector
completa, e ainda nao se alega superioridade ao TAA nativo sem A/B. Quando os
gates de escala passam, executa AMD FidelityFX FSR 1 EASU e RCAS de 0,4 stop.
Em resolucao nativa, RCAS atua diretamente depois do temporal.

O SRV final substitui somente scene-color naquele passe; GPS, textos, menus e
elementos desenhados depois permanecem nativos. Falha do hook/recurso registra
pass-through e continua observando. Nao existe tecla, preview ou ativacao
manual. Para R16F/R11, o shader usa SRTM reversivel; UNORM/sRGB segue direto.
As fontes GPUOpen FidelityFX-FSR v1.0.2 e a licenca MIT original acompanham o
pacote. A telemetria GPU registra TemporalAA, EASU e RCAS sem `Flush`.

O bootstrap reconhece exclusivamente `eurotrucks2.exe` e `amtrucks.exe`. Ele
cria `config.photorealism-native-aa.backup.cfg` no Documents do jogo e altera
atomicamente somente `r_aa=0`, `r_taa_tuning=0`,
`r_taa_luma_sharpen=0.0` e, se existir,
`r_taa_modulated_drr_strength=0.0`. Valores detectados/aplicados ficam em
`photorealism-plugin/photorealism-aa-config.log`. O plugin nao reativa AA/TAA
nativo em fallback. Se o jogo ja tiver lido o CFG, os valores persistidos
entram automaticamente na inicializacao seguinte, sem edicao ou tecla.

As calibracoes deixaram de ser um perfil absoluto sobrescrito a cada versao.
O tratamento visual continua em uma pilha cumulativa composta por:

- base 0.1.2;
- modulo visual 0.2.0;
- modulo de chuva e tempo nublado 0.3.0.

Com as tres camadas ativas, os valores efetivos sao os mesmos da 0.3.0. O
passe final oferece:

- temperatura de cor;
- exposicao e contraste;
- recuperacao de sombras e realces;
- pretos e brancos;
- saturacao e vibrance;
- microcontraste e nitidez;
- vinheta discreta.

O nucleo 0.11.0 intercepta `Present`, `Present1` quando a interface DXGI 1.2
esta disponivel, e `ResizeBuffers`. Antes de instalar, ele aguarda no maximo
tres segundos pelo `gameoverlayrenderer64.dll` e exige estabilizacao curta;
fora do Steam, instala normalmente pelo fallback limitado. `Present` e
`Present1` passam por um dispatcher reentrante que executa o tratamento uma
unica vez por cadeia de apresentacao. Enderecos, modulos proprietarios da
cadeia e possivel substituicao posterior sao auditados no log. Esta integracao
DXGI e nao uma API oficial do Prism3D ou do Steam.

Para tornar F12 deterministico, o core usa a interface oficial
`ISteamScreenshots` v003: registra `ScreenshotRequested_t`, e somente depois
ativa `HookScreenshots(true)`. A 0.10.4 admite somente um token de captura por
vez e coalesce callbacks repetidos do mesmo toque durante 750 ms. O overload de
call-result nao e tratado como outro `ScreenshotRequested_t`. Cada token aceito
agenda uma copia D3D11 para staging com query; `Map` ocorre apenas quando
pronta, a conversao RGB ocorre em um worker limitado a dois slots, e
`WriteScreenshot` e chamado uma unica vez no render thread. O log registra
`accepted`, `coalesced`, `write_handle` e `result` para provar a cardinalidade
no Proton. O core nunca le `VK_F12`. Se Steamworks, callback,
formato ou readback falharem, o hook e devolvido ao overlay e
`TriggerScreenshot` recupera a solicitacao sem uma captura paralela. Mudancas de resolucao, modo de
janela/tela cheia ou recriacao do swap chain invalidam somente os recursos
dependentes do backbuffer. Eles sao reconstruidos automaticamente no primeiro
frame valido seguinte.

Oito conjuntos de queries D3D11 medem a copia do backbuffer e o desenho do
shader. Os resultados sao consultados em frames posteriores sem `flush`. A
cada dez segundos, o log registra media, minimo, pico, amostras validas e
descartes. Se o driver nao oferecer timestamps, apenas a telemetria e
desativada; o efeito visual continua funcionando.

O nucleo tambem intercepta `OMSetRenderTargets` e sua variante com UAVs em
ciclos automaticos de descoberta. A versao 0.6.1 agrupa as depth-stencil views pela
textura D3D11 que realmente representam e aceita resolucoes internas diferentes
da resolucao da janela. Isso e necessario quando a escala de renderizacao do
ETS2 produz um mundo 3D maior ou menor que o backbuffer apresentado. Texturas e
grupos sao classificados por area, proporcao de tela e frequencia de uso. O log
informa ainda formato, MSAA, bind flags e se cada candidato pode ser lido
diretamente por shader. Depois da descoberta, o catalogo completo se desliga e
permanece somente um monitor leve do candidato selecionado. Uma mudanca de
resolucao inicia nova janela. Se um ciclo terminar ainda no menu sem encontrar
o depth da cena, a coleta inicia outro ciclo automaticamente e permanece pronta
para a entrada no mundo 3D. `End` e somente uma recarga manual opcional da
configuracao, dos shaders e da coleta; nao faz parte da inicializacao normal.

O teste da 0.6.1 identificou como candidato principal uma textura
`1920x2160`, `D32_FLOAT_S8X24_UINT`, sem MSAA e usada somente como
depth-stencil. A altura duplicada corresponde ao dimensionamento de 200% do
perfil testado. Como o recurso original nao permite leitura por shader, a
0.6.2 preserva-o intacto e cria uma textura auxiliar typeless com SRV
compativel. A copia ocorre depois de desassociar os render targets e antes do
passe final.

As imagens e o log da 0.6.2 confirmaram que esse recurso representa a camera
principal, esta alinhado ao frame final e usa reversed-Z. A 0.6.3 substitui a
hipotese forward-Z por dois modos conhecidos: reversed-Z realcado e distancia
linear. O modelo de plano distante infinito usa `distancia = near/depth`.
`near_plane` e a distancia exibida como branco ficam em uma secao diagnostica
separada no CFG; nao fazem parte da pilha de calibracao visual.

A 0.6.4 reduz o alcance do preview linear de `200` para `50` metros para
separar melhor a geometria proxima. Ela tambem reconstrui uma posicao
aproximada em espaco de camera a partir da profundidade e calcula normais pelos
pixels vizinhos. O parametro `vertical_fov` controla somente essa reconstrucao
diagnostica; ainda nao e uma matriz de projecao extraida do ETS2. As normais
servem para confirmar continuidade, orientacao e bordas antes de integrar um
efeito espacial.

Depois da validacao dessas normais, a 0.7.0 acrescenta o primeiro SSAO
experimental. O shader visual aprovado produz a mesma imagem de antes em uma
textura intermediaria; um novo shader separado combina essa imagem com a
profundidade. O primeiro perfil usa oito amostras, raio de `0.8` metro,
intensidade `0.28`, rejeicao de bordas e desaparecimento gradual entre `30` e
`70` metros. O objetivo e reforcar discretamente encontros entre superficies e
volumes proximos sem criar contornos escuros em silhuetas.

A 0.7.1 corrige o ciclo de vida desse depth durante transicoes entre menu e
mundo 3D. O SSAO so recebe uma textura que tenha sido vinculada pela propria
cena desde o frame anterior; chamadas feitas pelo passe do plugin nao contam.
Se o recurso deixar de ser usado, o SSAO e suspenso imediatamente e o passe
visual photorealista aprovado continua sozinho. Depois de 30 frames sem
atividade, o recurso antigo e liberado e uma nova descoberta comeca
automaticamente. Isso evita manchas com geometria da cena anterior sem exigir
que o usuario pressione `End`.

O log da validacao da 0.7.1 confirmou a recuperacao automatica, mas tambem
mostrou frames isolados nos quais o ETS2 atualizou a cena sem repetir o binding
do depth. A 0.7.2 passa a observar tambem `ClearDepthStencilView`, que representa
uma atualizacao efetiva do conteudo, e aceita uma tolerancia maxima de dois
frames. Essa tolerancia preserva a continuidade photorealista em variacoes
normais do pipeline; tres frames consecutivos sem atividade suspendem o SSAO e
30 frames invalidam o candidato, como protecao contra outra cena fantasma.

Com o SSAO experimental aprovado, a 0.8.0 inicia seu refinamento visual
photorealista. O modulo novo completa dois aneis simetricos com 16 amostras,
reduzindo padroes direcionais sem dobrar a intensidade media. A composicao
tambem passa a medir a luminancia linear da cena: farois, flares, gotas e
reflexos muito claros recebem menos escurecimento, enquanto contatos em pneus,
degraus, chassi, muros e piso continuam ganhando profundidade. A protecao e
gradual e preserva uma fracao da oclusao nas superficies claras para manter o
volume natural.

O refinamento fica em `[module.ssao_refinement.0.8.0]`. Desativa-lo retorna ao
algoritmo de oito amostras da 0.7.0 sem remover nenhuma camada anterior.

A 0.9.0 acrescenta o perfil `[module.ssao_interior.0.9.0]`. O ETS2 nao expoe
um marcador confiavel que diga ao plugin se a camera esta na cabine, por isso a
separacao e feita pela distancia linear: ate `near_start=2.0m`, detalhes de
primeiro plano usam raio menor, intensidade menor e rejeicao de bordas maior;
entre 2 e 8 metros a transicao e gradual; depois de `near_end=8.0m`, o perfil
exterior da 0.8.0 volta sem alteracao. Isso evita escurecer excessivamente
painel, colunas e retrovisores, sem perder profundidade nas estradas, veiculos
e ambientes externos.

Definir `enabled=false` apenas nessa secao retorna ao comportamento exterior
da 0.8.0. `End` recarrega a configuracao em tempo real; nao ha nova tecla.

A 0.9.1 corrige somente a descoberta do depth. O ranking anterior podia
favorecer um recurso `1920x1080` de interface pelo aspect ratio, mesmo quando o
depth interno `1920x2160` do ETS2 recebia mais de quatro vezes a quantidade de
bindings. Agora a atividade real tem maior peso, escalas internas assimetricas
sao aceitas e recursos de sombras quadrados continuam penalizados. Candidatos
com menos de `1000` bindings nao sao consolidados, evitando escolher o menu
antes da entrada no mundo 3D.

Durante a janela de 30 segundos, cada textura observada permanece referenciada
somente para que o vencedor por bindings possa ser retido com seguranca. Ao
final, todas as referencias perdedoras sao liberadas. Os shaders e todos os
valores visuais da 0.9.0 permanecem identicos.

A 0.10.0 inicia a integracao temporal sem substituir o TAA configurado no
jogo. Depois do tratamento visual e do SSAO, o plugin combina o frame atual
com um historico conservador. O historico e limitado pela vizinhanca 3x3 atual
e recebe rejeicao por diferenca de profundidade e de cor. Geometria revelada,
movimento de camera, luzes, chuva e reflexos que mudam rapidamente tendem,
portanto, a usar o frame atual; pixels estaveis podem acumular informacao para
reduzir cintilacao fina.

O modulo fica em `[module.temporal.0.10.0]`. Desativa-lo retorna a pilha
visual/SSAO consolidada na 0.9.1. O historico tambem e descartado
automaticamente em recarga, preview, resize, troca de depth e transicoes de
cena, evitando reutilizar informacao de outro quadro ou menu. Esta primeira
fase nao injeta jitter e nao possui vetores de movimento, portanto e uma
integracao segura posterior ao TAA nativo, nao um substituto completo dele.

A 0.10.1 corrige a inicializacao automatica dessa pilha. Antes, a unica janela
de descoberta podia expirar durante o carregamento ou no menu; nesse caso o
visual basico continuava ativo, mas SSAO e temporal permaneciam aguardando ate
uma recarga manual por `End`. Agora ciclos sem candidato se renovam sozinhos.
Ao detectar atividade sustentada de uma textura compatível com a cena 3D, o
plugin a consolida automaticamente depois de uma observacao minima curta. A
classificacao combina escala interna e taxa de bindings para rejeitar o depth
da interface do ETS2 sem depender da tecla.

Esta correcao nao altera nenhum parametro visual nem os shaders aprovados da
0.10.0. `Home`, `End` e `Insert` tambem passam a usar deteccao confiavel de
borda da tecla sob Proton, mas continuam sendo controles opcionais.

O shader visual aprovado continua no arquivo original e nao foi modificado.
Se a selecao do depth, criacao da copia, textura intermediaria ou shader SSAO
falhar, o plugin mantem automaticamente o passe visual normal.

Os valores iniciais foram calibrados de forma independente, usando as imagens
de referencia fornecidas como alvo visual. O arquivo de configuracao citado foi
usado somente para identificar familias de controles e faixas de calibracao.
Nenhum codigo ou binario de terceiros faz parte deste projeto.

## Instalacao de teste

Os arquivos `dinput8.dll`, `dxgi.dll`, `photorealism-fsr.dll` e a pasta
`photorealism-plugin` devem ficar em um dos diretorios correspondentes:

`Euro Truck Simulator 2/bin/win_x64/`

`American Truck Simulator/bin/win_x64/`

Nao e possivel manter dois proxies chamados `dinput8.dll` ou dois chamados
`dxgi.dll` no mesmo diretorio. Antes do teste, mova as duas DLLs do Snowymoon e
a pasta/config correspondente para um backup. Nao apague esses arquivos.

Atalhos:

- `Home`: ativa ou desativa o passe grafico;
- `End`: recarga manual opcional da configuracao, dos shaders e da descoberta
  depth; nao e necessario para ativar SSAO ou temporal.
- `Insert`: e a unica tecla do diagnostico; cada toque percorre `normal`,
  `raw`, `reversed-Z realcado`, `distancia linear`, `normais reconstruidas`,
  `mascara de visibilidade SSAO` e volta ao normal com SSAO.

O log sera criado em:

`bin/win_x64/photorealism-plugin/photorealism-plugin.log`

O diagnostico do dispositivo e dos alvos de cor sera criado separadamente em:

`bin/win_x64/photorealism-plugin/photorealism-fsr.log`

Quando o passe estiver realmente ativo, o log exibira as mensagens
`nucleo grafico carregado via dxgi.dll`, `Camadas cumulativas` e
`Primeiro frame processado`. A versao 0.11.0 aceita os backbuffers BGRA/RGBA
`UNORM` e `UNORM_SRGB`. O caminho normal usa views sRGB aceleradas pela GPU;
caso o driver nao permita alguma view, o shader usa conversao manual como
fallback.

## Limites desta fase

O SSAO usa uma projecao aproximada configurada por FOV. Tanto o resolve
consolidado 0.10.0 quanto o AA 0.6.0 nao possuem matriz de movimento, vetores
por pixel nem jitter proprio; o novo AA busca correspondencia local 3x3 e
rejeita historico em vez de fazer reprojecao completa. O proxy
ainda nao reproduz funcoes NGX exportadas pelo plugin de referencia. DLSS,
motion blur baseado em vetores, subsurface scattering, materiais de estrada,
chuva e espelhos nao fazem parte desta etapa. O Photorealism FSR 0.6.0 executa
EASU somente quando a engine apresenta uma fonte scene-color menor; temporal
e RCAS tambem operam no R11 nativo/supersampled comprovado por uso direto. O
modulo nao reduz por conta
propria a resolucao interna escolhida pelo Prism3D; sem essa fonte menor, nao
existe ganho de desempenho e o log registra pass-through. O roadmap esta em
`FSR_ROADMAP.md`.

## Compilacao no Linux

Requer Zig com suporte de cross-compilation para Windows GNU e um dos
utilitarios `zip`, `7z` ou `bsdtar`:

```bash
ZIG_BIN=/caminho/para/zig ./tools/package.sh
```
