# FSR Fase 2 — medir e otimizar na RX 6600 dentro do D3D11

## Contexto

A Fase 1 do plano `Implementação FSR no ETS.md` foi fechada no PR #5 (0.21.0–0.22.8):
EASU em compute, RCAS e LFGA iguais aos da AMD e testados no jogo. A Fase 2 do
plano diz "usar a GPU de verdade" e "otimizar para RDNA2, inclusive wave
operations quando fizer sentido".

O que a pesquisa mostrou antes de planejar:

- **o pseudocódigo da Fase 2 já roda**: `fsr_easu.hlsl` é compute com
  `numthreads(8, 8, 1)` e `dispatch_groups` = saída / 8 (`src/fsr/render_scale.cpp`); nada na CPU;
- **wave operations de 64 são Shader Model 6.6 (`[WaveSize]`) no DirectX 12** — README do clone
  da AMD, seção "64-wide wavefronts". O plugin é D3D11/SM 5.0 e o DXVK não expõe isso. A RX 6600 no
  RADV tem subgrupo 32/64 e float16, mas só seriam alcançáveis pela interop Vulkan do DXVK;
- **o número que a Fase 1 existia para produzir nunca foi medido**: tempo nativo contra upscale. Hoje
  o plugin só registra o custo do grade (`GpuTimer`, "Custo GPU do passe");
- **o jogo está com `r_vsync "1"`** em 60 Hz (e `t_limit_fps "120"`): o FPS fica preso e ganho não
  aparece sem uma sessão de medição sem vsync.

Decisões do usuário:
1. otimizar **dentro do D3D11**; wave operations ficam registradas como fora de alcance;
2. **float32 sempre** — o caminho de meia precisão da AMD fica fora; a imagem das otimizações tem de
   ser **bit a bit igual à 0.22.8**;
3. **sessão de medição**: o plugin registra FPS, 1% low, tempo de GPU do quadro e custo do FSR; o
   usuário desliga vsync e limitador só durante a sessão e compara nativo contra 75/56/44%.

Regra que governa a fase: nenhuma otimização entra sem medida que mostre ganho, e nenhuma muda um bit
da imagem.

## Etapa 1 — Instrumentação (pacote 0.23.0)

Branch `fsr-fase2-0.23.0` a partir da `main`. O ROADMAP renumera raios de sol para 0.24.0 e o bind
flag condicional para 0.25.0 (número não entregue não reserva; o validate cobra).

**1.1 Ritmo de quadros (CPU).** `QueryPerformanceCounter` no início de `hooked_present` e
`hooked_present1` (`src/hooks/swap_chain_hooks.cpp`), antes de `upscale_present_frame`. O intervalo
entre apresentações alimenta uma classe pura nova `FramePacing` (`src/telemetry/frame_pacing.hpp`,
sem `windows.h`): janela de 10 s com FPS médio, tempo médio, **1% low = 1000 / média dos 1% quadros
mais lentos**, 0,1% low e pior quadro. Teste de host com resposta conhecida (sequência sintética com
os quadros lentos plantados).

**1.2 Tempo de GPU entre apresentações.** Reusar `GpuTimer` (`src/postprocess/gpu_timer.{hpp,cpp}`):
um slot aberto numa apresentação e fechado na seguinte, com a próxima abrindo no mesmo ponto. O
`GpuTimer` ganha um rótulo para o log deixar de ser sempre "Custo GPU do passe". Ressalva que vai no
log e no documento: com a GPU ociosa esse intervalo inclui espera; ele é tempo de GPU do quadro só
quando a GPU é o gargalo — por isso a sessão é sem vsync.

**1.3 Custo do FSR.** Três `GpuTimer` rotulados: cópia do quadro interno (`ColorCapture::copy_from`,
`src/resource_observer/color_capture.cpp`), EASU (`UpscalePipeline::dispatch_easu`) e RCAS+LFGA
(`UpscalePipeline::draw_rcas`, `src/fsr/upscale_pipeline.cpp`). Mais o custo de **CPU** do FSR por
quadro: `QueryPerformanceCounter` acumulado dentro de `observe_color_targets` e de
`reconstruct_game_frame` (roda em todo `OMSetRenderTargets` do jogo com o FSR ligado).

**1.4 Marcador da sessão.** Tecla **Scroll Lock** (não usada pelo ETS2) inicia e encerra um trecho
medido, tratada em `handle_hotkeys()` (`src/postprocess/postprocessor.cpp`) ao lado de Home/End/
Insert, pelo mesmo `key_pressed_once`. Ao encerrar, uma linha de resumo do trecho: configuração
(FSR ligado?, escala, interno x saída), FPS médio, 1% low, 0,1% low, tempo de GPU médio e os custos
do FSR. Sem marcador, as janelas de 10 s continuam no log.

**1.5 Protocolo.** `references/benchmark-fsr-fase2.md`: mesmo save e mesmo trecho de estrada (~2 min),
vsync e limitador desligados no menu do jogo só para a sessão, quatro rodadas — nativo (FSR
desligado, escala 100%), 0.8660, 0.75, 0.6667 — cada uma com reinício do jogo (a escala só vale no
início) e Scroll Lock no começo e no fim do trecho. O plugin não toca em vsync nem limitador.

## Etapa 2 — Análise

Com os logs das quatro rodadas: a tabela nativo × 75/56/44% (FPS, 1% low, tempo de GPU) — o número da
Fase 1 — e a divisão do custo do FSR (cópia, EASU, RCAS+LFGA, CPU dos hooks). A análise decide quais
candidatos da Etapa 3 valem a pena; um custo que não aparecer na medida não é otimizado.

## Etapa 3 — Otimizações candidatas (0.23.x), cada uma só se a medida justificar

Todas preservam a imagem bit a bit; as que tocam GPU são comprovadas contra a saída da 0.22.8 no
harness de GPU (wine + DXVK do Proton, RX 6600, mesmo método da 0.22.4/0.22.6).

1. **Tamanho do grupo de compute** (8×8 contra 16×16 e 32×8): muda só o despacho, não o resultado.
   O `numthreads(8, 8, 1)` é o que o plano do usuário escreveu e uma guarda do validate prende
   (`tools/validate.sh:1609`); se outro tamanho ganhar, o número vai para o usuário decidir antes.
2. **CPU dos hooks**: `describe_view` faz `GetResource` + `QueryInterface` + `GetDesc` em todo bind
   (`src/resource_observer/view_shape.cpp`); cache por ponteiro de view invalidado na troca de
   dispositivo e no resize, se o custo de CPU medido for relevante.
3. **Constantes do EASU**: `UpdateSubresource` só quando interno ou saída mudam.
4. **Cópia do quadro interno**: só se a medida mostrar custo relevante. Registrar o limite: a textura
   do jogo é `R8G8B8A8_UNORM_SRGB` tipada, então ler direto dela devolveria valores linearizados e
   mudaria os bits — fora da regra; a alternativa precisa manter o dado cru.

Após cada otimização aceita: harness bit a bit, `build.sh`, `validate.sh`, e a rodada de medição
repetida para a configuração afetada.

## Arquivos

- novos: `src/telemetry/frame_pacing.{hpp,cpp}`, `src/telemetry/measurement_session.{hpp,cpp}`,
  `tests/frame_pacing_test.cpp`, `references/benchmark-fsr-fase2.md`,
  `references/fsr-fase2-wave-operations.md` (por que wave ops ficam fora, com a fonte da AMD e o
  `vulkaninfo` da RX 6600)
- alterados: `src/hooks/swap_chain_hooks.cpp`, `src/postprocess/gpu_timer.{hpp,cpp}`,
  `src/postprocess/postprocessor.cpp`, `src/fsr/upscale_pipeline.cpp`,
  `src/resource_observer/color_capture.cpp`, `src/resource_observer/color_observation.cpp`,
  `tools/build.sh`, `tools/validate.sh`, `tools/package.sh`, `CHANGELOG.md`, `ROADMAP.md`

Regras da casa mantidas: SRP, sem comentários no fonte, ≤ ~200 linhas por arquivo novo, log em
português sem acento, commit só com build + validate verdes.

## Verificação

- **Host**: `frame_pacing_test` com resposta conhecida (1% low e 0,1% low de uma sequência plantada);
  registrado no validate.
- **Guardas** novas (quebradas de propósito numa cópia, uma a uma): marcador tratado em
  `handle_hotkeys`; `FramePacing` alimentado nos dois Present; timers em volta da cópia, do EASU e do
  RCAS; rótulo do `GpuTimer` usado.
- **Custo da instrumentação**: queries de timestamp e QPC não podem mudar a imagem — harness compara a
  saída do FSR com e sem timers.
- **No jogo**: sessão do protocolo; o log tem de mostrar as quatro rodadas com resumo do trecho
  marcado. O resultado é a tabela nativo × FSR, que vira a primeira seção do documento de benchmark.
