#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dinput_dll="${project_dir}/build/dinput8.dll"
dxgi_dll="${project_dir}/build/dxgi.dll"
fsr_dll="${project_dir}/build/photorealism-fsr.dll"

for dll in "${dinput_dll}" "${dxgi_dll}" "${fsr_dll}"; do
  if [[ ! -f "${dll}" ]]; then
    echo "DLL ausente: execute tools/build.sh primeiro: ${dll}" >&2
    exit 1
  fi
  grep -q 'PE32+.*DLL.*x86-64' <<<"$(file "${dll}")"
done

# As tabelas de literais abaixo sao consultadas dezenas de vezes por DLL. Um
# "strings ... | grep -Fq" encerra o grep no primeiro acerto, o strings recebe
# SIGPIPE e o pipeline sai 141: sob "set -o pipefail" isso reprova a checagem
# justamente quando ela passa. Extrair uma vez para arquivo elimina o SIGPIPE
# e evita reexecutar o strings a cada literal.
strings_cache="$(mktemp -d)"
trap 'rm -rf -- "${strings_cache}"' EXIT
dxgi_strings="${strings_cache}/dxgi.txt"
fsr_strings="${strings_cache}/photorealism-fsr.txt"
strings "${dxgi_dll}" >"${dxgi_strings}"
strings "${fsr_dll}" >"${fsr_strings}"

dinput_metadata="$(objdump -p "${dinput_dll}")"
expected_dinput_exports=(
  '1:DirectInput8Create'
  '2:DllRegisterServer'
  '3:DllUnregisterServer'
  '4:DllCanUnloadNow'
  '5:DllGetClassObject'
  '6:GetdfDIJoystick'
)

for expected in "${expected_dinput_exports[@]}"; do
  ordinal="${expected%%:*}"
  name="${expected#*:}"
  if ! grep -Eq "\\+base\\[[[:space:]]*${ordinal}\\].*[[:space:]]${name}$" \
      <<<"${dinput_metadata}"; then
    echo "Export dinput8 ausente ou com ordinal incorreto: ${name} @${ordinal}" >&2
    exit 1
  fi
done

if grep -Eqi 'DLL Name: (d3d11|dxgi|d3dcompiler)' <<<"${dinput_metadata}"; then
  echo "O bootstrap dinput8 possui dependencia grafica inesperada." >&2
  exit 1
fi

dxgi_metadata="$(objdump -p "${dxgi_dll}")"
expected_dxgi_exports=(
  '1:CreateDXGIFactory'
  '2:CreateDXGIFactory1'
  '3:CreateDXGIFactory2'
)

for expected in "${expected_dxgi_exports[@]}"; do
  ordinal="${expected%%:*}"
  name="${expected#*:}"
  if ! grep -Eq "\\+base\\[[[:space:]]*${ordinal}\\].*[[:space:]]${name}$" \
      <<<"${dxgi_metadata}"; then
    echo "Export dxgi ausente ou com ordinal incorreto: ${name} @${ordinal}" >&2
    exit 1
  fi
done

if grep -Eqi 'DLL Name: dxgi\.dll' <<<"${dxgi_metadata}"; then
  echo "O proxy dxgi possui uma auto-dependencia recursiva." >&2
  exit 1
fi

if grep -Fqi 'DLL Name: photorealism-fsr.dll' <<<"${dxgi_metadata}"; then
  echo "O nucleo possui dependencia estatica do modulo FSR opcional." >&2
  exit 1
fi

fsr_metadata="$(objdump -p "${fsr_dll}")"
if ! grep -Eq \
    '\+base\[[[:space:]]*1\].*[[:space:]]PhotorealismFsrGetApi$' \
    <<<"${fsr_metadata}"; then
  echo "Export FSR ausente ou com ordinal incorreto: PhotorealismFsrGetApi @1" >&2
  exit 1
fi

for forbidden_export in \
  'CreateDXGIFactory' \
  'CreateDXGIFactory1' \
  'CreateDXGIFactory2' \
  'DirectInput8Create'; do
  if grep -Eq "[[:space:]]${forbidden_export}$" <<<"${fsr_metadata}"; then
    echo "O modulo auxiliar FSR nao pode atuar como proxy: ${forbidden_export}" >&2
    exit 1
  fi
done

if grep -qi 'snowymoon' \
    <<<"${dinput_metadata}${dxgi_metadata}${fsr_metadata}"; then
  echo "Dependencia inesperada de Snowymoon encontrada." >&2
  exit 1
fi

for core_fsr_message in \
  'Photorealism FSR/AA 0.7.1: modulo auxiliar ausente ou indisponivel' \
  'nucleo 0.11.0 continua normalmente' \
  'PhotorealismFsrGetApi' \
  'ABI incompativel ou incompleta' \
  'modulo auxiliar carregado' \
  'color_observer=%s automatic_selection=%s aa_easu_rcas=%s draw_proof=%s raster_shadow=%s' \
  'dispositivo real do jogo entregue ao modulo' \
  'inicializacao do dispositivo falhou' \
  'dispositivo encerrado para reinicializacao segura'; do
  if ! grep -Fq "${core_fsr_message}" "${dxgi_strings}"; then
    echo "Integracao FSR ausente no nucleo DXGI: ${core_fsr_message}" >&2
    exit 1
  fi
done

for module_fsr_message in \
  'Photorealism FSR/AA 0.7.1 inicializado' \
  'ABI=v1+v2+v3+v4+v5+v6' \
  'feature_level=%s' \
  '12_1' \
  'Adapter: name=' \
  'Capacidades D3D11' \
  'R16G16B16A16_FLOAT' \
  'R11G11B10_FLOAT' \
  'R8G8B8A8_UNORM' \
  'Observador color 0.7.1 pronto' \
  'diagnostic_queue=2 worker=%s' \
  'Janela color 0.7.0 %s' \
  'Relatorio color 0.7.0' \
  'async_job_drops=%llu' \
  'report_queue_drops=%llu' \
  'Color target #%llu' \
  'presentation-evidence' \
  'probable-scene' \
  'probable-mirror-reflection' \
  'probable-interface' \
  'consultas COM apenas em cache miss' \
  'AA/FSR source selecionado' \
  'fallback pass-through' \
  'selecao automatica estabilizada por composicao direta' \
  'AA Photorealism ativo antes da UI' \
  'temporal=history-clamp+screenspace-3x3' \
  'engine_motion_vectors=indisponiveis jitter=indisponivel' \
  'Telemetria GPU AA/FSR 0.7.0' \
  'Draw proof 0.7.1' \
  'raster_seed=%llu' \
  'Final draw proof locked 0.7.1' \
  'Rejected draw signature' \
  'TemporalAA_avg=%.3fms' \
  '0.4-stops' \
  'worker diagnostico drenado com seguranca'; do
  if ! grep -Fq "${module_fsr_message}" "${fsr_strings}"; then
    echo "Diagnostico ausente no modulo FSR: ${module_fsr_message}" >&2
    exit 1
  fi
done

build_script="${project_dir}/tools/build.sh"

# A lista de TUs do photorealism-fsr.dll sai do proprio build.sh, que e a fonte
# autoritativa: dividir o modulo em mais arquivos passa a estender as checagens
# de fonte abaixo automaticamente, sem manutencao paralela aqui.
mapfile -t fsr_relative_sources < <(awk '
  /^"\$\{zig_bin\}" c\+\+/ { count = 0; next }
  match($0, /src\/[A-Za-z0-9_.-]+\.cpp/) {
    pending[++count] = substr($0, RSTART, RLENGTH)
  }
  /-o "\$\{build_dir\}\/photorealism-fsr\.dll"/ {
    for (index_ = 1; index_ <= count; ++index_) {
      print pending[index_]
    }
    exit
  }
' "${build_script}")

if [[ "${#fsr_relative_sources[@]}" -lt 2 ]] ||
    [[ ! " ${fsr_relative_sources[*]} " == *" src/fsr_module.cpp "* ]]; then
  echo "Nao foi possivel extrair as fontes do photorealism-fsr.dll do build.sh." >&2
  exit 1
fi

fsr_sources=()
for relative in "${fsr_relative_sources[@]}"; do
  if [[ ! -f "${project_dir}/${relative}" ]]; then
    echo "Fonte declarada em build.sh nao existe: ${relative}" >&2
    exit 1
  fi
  fsr_sources+=("${project_dir}/${relative}")
done

# Um .cpp novo que nunca foi registrado em build.sh nao seria compilado nem
# validado; a ausencia silenciosa e exatamente o que esta checagem impede.
for candidate in "${project_dir}"/src/fsr_*.cpp; do
  if ! grep -Fq "src/$(basename "${candidate}")" "${build_script}"; then
    echo "Fonte FSR ausente em build.sh: ${candidate}" >&2
    exit 1
  fi
done

fsr_grep() {
  grep -Fq "$1" "${fsr_sources[@]}"
}

# Extrai o corpo de uma funcao a partir de sua assinatura. Tres defesas contra o
# falso verde que o "sed -n /assinatura/,/^}/p" original permitia:
#   - ancora em coluna 1, para um call site indentado nao abrir o intervalo;
#   - rejeita declaracao forward, que abriria o intervalo e correria ate o "}"
#     da proxima funcao, devolvendo um corpo completamente errado;
#   - conta os acertos e reprova se != 1, para que "mudou de arquivo" ou
#     "sumiu" viraem falha em vez de corpo vazio com checagem negativa passando.
extract_function() {
  local signature="$1"
  local hits=0 body="" source found
  for source in "${fsr_sources[@]}"; do
    found="$(awk -v sig="${signature}" '
      index($0, sig) == 1 && $0 !~ /;[[:space:]]*$/ { inside = 1 }
      inside { print }
      inside && /^}/ { exit }
    ' "${source}")"
    if [[ -n "${found}" ]]; then
      hits=$((hits + 1))
      body="${found}"
    fi
  done
  if [[ "${hits}" -ne 1 ]]; then
    echo "Assinatura FSR nao definida exatamente uma vez: ${signature} (${hits})" >&2
    exit 1
  fi
  printf '%s\n' "${body}"
}

# Sentinela obrigatoria: prova que o corpo extraido e mesmo o corpo esperado,
# para que uma extracao vazia ou mal ancorada nao passe nas checagens negativas.
assert_body_sentinel() {
  if ! grep -Fq "$3" <<<"$2"; then
    echo "Sentinela ausente no corpo de ${1}: $3" >&2
    exit 1
  fi
}

observe_frame_source="$(extract_function 'void WINAPI observe_frame(')"
assert_body_sentinel 'observe_frame' "${observe_frame_source}" \
  'register_backbuffer_locked('
for forbidden_present_work in \
  'log_message(' \
  'std::sort' \
  'CreateFileW' \
  'WriteFile' \
  'CloseHandle'; do
  if grep -Fq "${forbidden_present_work}" <<<"${observe_frame_source}"; then
    echo "Trabalho bloqueante voltou ao observe_frame: ${forbidden_present_work}" >&2
    exit 1
  fi
done

automatic_selection_source="$(extract_function \
  'HRESULT WINAPI update_automatic_selection(')"
assert_body_sentinel 'update_automatic_selection' \
  "${automatic_selection_source}" 'score_automatic_scene_candidate('
for forbidden_hot_path_work in \
  'log_message(' \
  'new ' \
  'CreateFileW' \
  'WriteFile' \
  'CloseHandle' \
  'GetResource(' \
  'QueryInterface('; do
  if grep -Fq "${forbidden_hot_path_work}" <<<"${automatic_selection_source}"; then
    echo "Trabalho indevido voltou a selecao automatica: ${forbidden_hot_path_work}" >&2
    exit 1
  fi
done

if ! fsr_grep 'kSrvReplacementRequiresDrawProof'; then
  echo "O caminho ABI v4 deixou de falhar fechado sem prova de draw." >&2
  exit 1
fi

for draw_proof_marker in \
  'void WINAPI observe_pixel_shader_resources' \
  'void WINAPI observe_rasterizer_state' \
  'void WINAPI observe_viewports' \
  'void WINAPI observe_scissor_rects' \
  'void WINAPI observe_final_draw' \
  'OMGetRenderTargets' \
  'PSGetShaderResources' \
  'PSGetShader' \
  'IAGetPrimitiveTopology' \
  'record_rejected_draw_locked' \
  'rejected_draw_identity_matches' \
  'samples=first-occurrence' \
  'Rejected draw signature' \
  'kFinalDrawProofConfirmFrames = 24' \
  'replacement=0 dispatch=0'; do
  if ! fsr_grep "${draw_proof_marker}"; then
    echo "Validacao de draw final incompleta: ${draw_proof_marker}" >&2
    exit 1
  fi
done

observe_final_draw_source="$(extract_function 'void WINAPI observe_final_draw(')"
assert_body_sentinel 'observe_final_draw' "${observe_final_draw_source}" \
  'record_rejected_draw_locked('
for forbidden_raster_query in \
  'RSGetState' \
  'RSGetViewports' \
  'RSGetScissorRects'; do
  if grep -Fq "${forbidden_raster_query}" <<<"${observe_final_draw_source}"; then
    echo "Consulta rasterizadora ao vivo indevida na prova passiva: ${forbidden_raster_query}" >&2
    exit 1
  fi
done

# Espelho positivo do bloco acima: proibir os RSGet* na prova nao pode virar
# desculpa para nao te-los em lugar nenhum. Sem a semeadura sob demanda, um
# contexto que nunca chamou RSSetState ficaria eternamente desconhecido.
raster_shadow_source="${project_dir}/src/fsr_rasterizer_shadow.cpp"
for lazy_seed_marker in \
  'RSGetState' \
  'RSGetViewports' \
  'RSGetScissorRects' \
  'rasterizer_shadow_mark_stale' \
  'seeded_from_live_state'; do
  if ! grep -Fq "${lazy_seed_marker}" "${raster_shadow_source}"; then
    echo "Semeadura preguicosa do shadow rasterizador ausente: ${lazy_seed_marker}" >&2
    exit 1
  fi
done

hook_source="${project_dir}/src/hook.cpp"
for raster_shadow_hook in \
  '&context_vtable[43]' \
  '&context_vtable[44]' \
  '&context_vtable[45]' \
  'hooked_rs_set_state' \
  'hooked_rs_set_viewports' \
  'hooked_rs_set_scissor_rects'; do
  if ! grep -Fq "${raster_shadow_hook}" "${hook_source}"; then
    echo "Hook de shadow rasterizador ausente: ${raster_shadow_hook}" >&2
    exit 1
  fi
done

# "grep -Fc" conta por arquivo; com varios fontes a soma correta exige -Foh.
uav_increments="$(grep -Foh '++g_uav_event_count;' "${fsr_sources[@]}" | wc -l)"
if [[ "${uav_increments}" -ne 1 ]]; then
  echo "uav_event_count deve ser incrementado exatamente uma vez por evento UAV." >&2
  exit 1
fi

for async_architecture_marker in \
  'kDiagnosticQueueCapacity = 2' \
  'DWORD WINAPI diagnostic_worker' \
  'WaitForMultipleObjects' \
  'write_color_report(&job.snapshot' \
  'SleepConditionVariableSRW' \
  'take_report_snapshot(&job.snapshot)'; do
  if ! fsr_grep "${async_architecture_marker}"; then
    echo "Arquitetura assincrona FSR ausente: ${async_architecture_marker}" >&2
    exit 1
  fi
done

if ! grep -Fq \
    'Hooks Present/Present1/ResizeBuffers/OMSetRenderTargets*/ClearDepthStencilView instalados; PSSetShaderResources=%s' \
    "${dxgi_strings}"; then
  echo "Conjunto de hooks DXGI/D3D11 incompleto no nucleo." >&2
  exit 1
fi


for core_0110_message in \
  'Photorealism Plugin 0.11.0' \
  'gameoverlayrenderer64.dll' \
  'Steam overlay detectado e estabilizado antes dos hooks' \
  'fallback nao-Steam' \
  'Present1=%s' \
  'ativo-slot22' \
  'Auditoria Present 0.11.0' \
  'nosso-hook-externo' \
  'substituido-ou-encadeado' \
  'post-install-5000ms'; do
  if ! grep -Fq "${core_0110_message}" "${dxgi_strings}"; then
    echo "Core overlay-aware 0.11.0 incompleto: ${core_0110_message}" >&2
    exit 1
  fi
done

# F12 e a tecla de screenshot do Steam e continua sendo dele. Quem garante
# isso e o HookScreenshots(true) verificado mais abaixo, nao uma proibicao do
# literal VK_F12 no codigo -- a captura e delegada ao Steam por API.
#
# A unica invariante necessaria aqui e que o modulo de captura nao passe a
# disputar teclado com o Steam.
if rg -n 'VK_|GetAsyncKeyState' \
    "${project_dir}/src/steam_screenshots.cpp" >/dev/null; then
  echo "A captura Steam nao pode consultar teclado." >&2
  exit 1
fi

# Page Up alterna o RTGI desde a 0.12.0, e so isso: precisa aparecer uma unica
# vez, no passe de pos-processamento.
if rg -n --glob '!postprocess.cpp' 'VK_PRIOR|PageUp' \
    "${project_dir}/src" >/dev/null; then
  echo "Page Up so pode existir no passe de pos-processamento." >&2
  exit 1
fi
if [[ "$(grep -c 'VK_PRIOR' "${project_dir}/src/postprocess.cpp")" != "1" ]]; then
  echo "Page Up precisa aparecer exatamente uma vez no postprocess." >&2
  exit 1
fi

# Page Down cicla as debug views do RTGI, com a mesma regra.
if rg -n --glob '!postprocess.cpp' 'VK_NEXT|PageDown' \
    "${project_dir}/src" >/dev/null; then
  echo "Page Down so pode existir no passe de pos-processamento." >&2
  exit 1
fi
if [[ "$(grep -c 'VK_NEXT' "${project_dir}/src/postprocess.cpp")" != "1" ]]; then
  echo "Page Down precisa aparecer exatamente uma vez no postprocess." >&2
  exit 1
fi

if rg -n 'void Run\(void\* parameter, bool, std::uint64_t\).*Run\(parameter\)' \
    -U "${project_dir}/src/steam_screenshots.cpp" >/dev/null; then
  echo "Callback Steam call-result nao pode criar outra captura." >&2
  exit 1
fi

for screenshot_gate_marker in \
  'g_capture_cycle_active.compare_exchange_strong' \
  'request_is_duplicate(now, previous, false)' \
  'g_requests.store(1u' \
  'g_capture_cycle_active.store(false'; do
  if ! grep -Fq "${screenshot_gate_marker}" \
      "${project_dir}/src/steam_screenshots.cpp"; then
    echo "Token/deduplicacao de screenshot ausente: ${screenshot_gate_marker}" >&2
    exit 1
  fi
done

for steam_screenshot_message in \
  'SteamAPI_SteamScreenshots_v003' \
  'SteamAPI_ISteamScreenshots_HookScreenshots' \
  'SteamAPI_ISteamScreenshots_WriteScreenshot' \
  'SteamAPI_ISteamScreenshots_TriggerScreenshot' \
  'ScreenshotRequested_t=2302' \
  'exatamente um WriteScreenshot por toque' \
  'deduplicacao=750ms' \
  'write_handle=%u' \
  'accepted=%llu coalesced=%llu' \
  'result=ok' \
  'F12 nativo preservado'; do
  if ! grep -Fq "${steam_screenshot_message}" "${dxgi_strings}"; then
    echo "Integracao Steam screenshot incompleta: ${steam_screenshot_message}" >&2
    exit 1
  fi
done

if ! rg -n 'process_frame\(swap_chain\);[[:space:]]*observe_postprocessed_frame\(swap_chain\);' \
    -U "${project_dir}/src/hook.cpp" >/dev/null; then
  echo "Fronteira pos-processada ausente depois de todos os passes visuais." >&2
  exit 1
fi

for home_gate_marker in \
  'set_fsr_processing_enabled(settings_.enabled)' \
  'fsr_processing_enabled()' \
  'PHOTOREALISM_FSR_RESET_PLUGIN_DISABLED'; do
  if ! rg -Fq "${home_gate_marker}" "${project_dir}/src"; then
    echo "Gate Home para AA/FSR ausente: ${home_gate_marker}" >&2
    exit 1
  fi
done

for observer_message in \
  'Descoberta depth 0.10.1 %s' \
  'early=%llums' \
  'iniciada por backbuffer' \
  'reiniciada via End' \
  'reiniciada por troca de dispositivo' \
  'reiniciada automaticamente por depth obsoleto' \
  'Descoberta depth 0.10.1 concluida' \
  'mode=%s' \
  'minimum_bindings=%llu' \
  'minimum_rate=%llu/s' \
  'scaled_area=%llu%%' \
  'Depth grupo' \
  'Depth recurso' \
  'Depth principal 0.10.1 selecionado automaticamente' \
  'descoberta continuara automaticamente' \
  'Recursos de copia depth criados' \
  'Depth sem atividade confirmada por %u frames' \
  'Depth obsoleto invalidado apos %u frames' \
  'Preview depth ativo' \
  'Depth linearization 0.6.4' \
  'reversed-z-enhanced' \
  'linear-distance' \
  'reconstructed-normals' \
  'ssao-visibility' \
  'Modulo SSAO 0.7.0' \
  'Modulo SSAO refinement 0.8.0' \
  'Modulo SSAO interior 0.9.0' \
  'Modulo temporal 0.10.0' \
  'SSAO 0.9.1 ativo' \
  'ssao_0.9.1' \
  'Resolve temporal 0.10.0 ativo' \
  'Historico temporal 0.10.0 inicializado' \
  'temporal_0.10.0'; do
  if ! grep -Fq "${observer_message}" "${dxgi_strings}"; then
    echo "Observador de profundidade ausente: ${observer_message}" >&2
    exit 1
  fi
done

for telemetry_message in \
  'Telemetria GPU inicializada' \
  'Custo GPU do passe'; do
  if ! grep -Fq "${telemetry_message}" "${dxgi_strings}"; then
    echo "Telemetria ausente no nucleo DXGI: ${telemetry_message}" >&2
    exit 1
  fi
done

cfg="${project_dir}/config/photorealism-plugin.cfg"
for section in \
  '[base.0.1.2]' \
  '[module.visual.0.2.0]' \
  '[module.rain_overcast.0.3.0]' \
  '[depth.0.6.4]' \
  '[module.ssao.0.7.0]' \
  '[module.ssao_refinement.0.8.0]' \
  '[module.ssao_interior.0.9.0]' \
  '[module.temporal.0.10.0]'; do
  grep -Fqx "${section}" "${cfg}"
done
grep -Fqx 'near_plane=0.1' "${cfg}"
grep -Fqx 'preview_distance=50.0' "${cfg}"
grep -Fqx 'vertical_fov=60.0' "${cfg}"
grep -Fqx 'radius=0.8' "${cfg}"
grep -Fqx 'intensity=0.28' "${cfg}"
grep -Fqx 'bias=0.04' "${cfg}"
grep -Fqx 'fade_start=30.0' "${cfg}"
grep -Fqx 'fade_end=70.0' "${cfg}"
grep -Fqx 'edge_rejection=1.5' "${cfg}"
grep -Fqx 'highlight_start=0.55' "${cfg}"
grep -Fqx 'highlight_end=0.95' "${cfg}"
grep -Fqx 'highlight_ao_floor=0.35' "${cfg}"
grep -Fqx 'near_start=2.0' "${cfg}"
grep -Fqx 'near_end=8.0' "${cfg}"
grep -Fqx 'radius=0.45' "${cfg}"
grep -Fqx 'intensity=0.20' "${cfg}"
grep -Fqx 'bias=0.05' "${cfg}"
grep -Fqx 'edge_rejection=1.75' "${cfg}"
grep -Fqx 'history_weight=0.65' "${cfg}"
grep -Fqx 'depth_rejection=0.02' "${cfg}"
grep -Fqx 'color_rejection=0.08' "${cfg}"

# Modulo RTGI 0.12.0. Os valores vem do documento da tecnica; enabled=false
# porque esta versao e andaime e nao traca raio nenhum.
grep -Fqx '[module.rtgi.0.12.0]' "${cfg}"
grep -Fqx 'resolution_scale=2' "${cfg}"
grep -Fqx 'ray_count=4' "${cfg}"
grep -Fqx 'max_steps=12' "${cfg}"
grep -Fqx 'range_min=0.10' "${cfg}"
grep -Fqx 'range_max=15.0' "${cfg}"
# gi_intensity multiplica o GI inteiro na composicao, e ate a 0.13.2.1 valia
# 0.15. Com sky_ambient tambem multiplicando, o teto do que um raio escapado
# somava era 0,0375 linear e o raio tipico no painel ficava em ~0,008 -- cerca
# de 5 niveis em 255. O efeito existia e nao dava para ver.
if grep -Eq '^gi_intensity=0\.[0-2][0-9]*$' "${cfg}"; then
  echo "gi_intensity voltou para a faixa em que o GI e invisivel: com \
sky_ambient multiplicando junto, o painel ganha uns 5 niveis em 255." >&2
  exit 1
fi
grep -Fqx 'gi_intensity=0.6' "${cfg}"
grep -Fqx 'max_indirect_luma=4.0' "${cfg}"
# sky_ambient e a luz de um raio que escapa sem acertar nada. Em zero (o valor
# ate a 0.13.2.1) o shader responde preto a todo "nao sei", e dentro da cabine
# escapam todos os raios: o hemisferio de uma superficie virada para a camera e
# o ar entre ela e o olho. Painel preto chapado dentro de um tunel iluminado.
if grep -Eq '^sky_ambient=0(\.0+)?$' "${cfg}"; then
  echo "sky_ambient voltou a zero: todo raio que escapa sem acertar nada volta \
a devolver preto, e na cabine escapam todos." >&2
  exit 1
fi
grep -Fqx 'sky_ambient=0.25' "${cfg}"
# Os quatro parametros da acumulacao temporal, ativos desde a 0.13.3. As tres
# rejeicoes sao multiplicadas em PSRtgiTemporal: qualquer uma zerada aceitaria
# a historia de qualquer superficie, que e ghosting, e ghosting e o unico
# defeito que a acumulacao introduz e nao consegue desfazer.
rtgi_temporal_zero='^(history_weight|depth_rejection|normal_rejection'
rtgi_temporal_zero="${rtgi_temporal_zero}|color_rejection)=0(\.0+)?$"
if grep -Eq "${rtgi_temporal_zero}" "${cfg}"; then
  echo "Parametro da acumulacao RTGI zerado: as tres rejeicoes sao \
multiplicadas, e uma delas em zero descarta ou aceita tudo." >&2
  exit 1
fi
for rtgi_temporal_pin in \
  'history_weight=0.90' \
  'depth_rejection=0.015' \
  'normal_rejection=0.85' \
  'color_rejection=0.05'; do
  if ! grep -Fqx "${rtgi_temporal_pin}" "${cfg}"; then
    echo "Parametro da acumulacao RTGI 0.13.3 fora do valor aprovado: \
${rtgi_temporal_pin}" >&2
    exit 1
  fi
done
# As marcas de inercia saem junto com a entrega que as consome.
for retired_marker in 'INERTE ate a 0.13.2' 'INERTE ate a 0.13.3'; do
  if grep -Fq "${retired_marker}" "${cfg}"; then
    echo "cfg ainda marca como inertes valores ja ligados: \
${retired_marker}" >&2
    exit 1
  fi
done
# Politica de AA nativa: o TAA ligado e o que expoe o depth ao shader.
grep -Fqx '[native_aa.0.12.2]' "${cfg}"
grep -Fqx 'manage=true' "${cfg}"
grep -Fqx 'r_aa=6' "${cfg}"
grep -Fqx 'r_taa_luma_sharpen=1.5' "${cfg}"

grep -Fqx 'hit_thickness=0.5' "${cfg}"
grep -Fqx 'normal_bias=0.05' "${cfg}"
grep -Fqx 'debug=final' "${cfg}"

# O modulo RTGI 0.12.0 precisa estar compilado no nucleo, incluindo a tecla
# Page Up e o aviso de que nenhum raio e tracado nesta versao.
for rtgi_message in \
  'RTGI 0.13.2 %s pelo atalho Page Up.' \
  'Preview RTGI 0.13.1 pelo Page Down: debug=%s%s.' \
  'Insert na posicao 6 para desenhar' \
  'Recursos RTGI 0.13.2 criados' \
  'Falha ao criar recursos RTGI 0.13.2' \
  'Falha ao criar alvo de composicao RTGI 0.13.2' \
  'marcha geometrica, o GI e somado a cena antes do grading' \
  'hit_distance' \
  'rtgi_0.13.2=%s' \
  'rtgi_acumulacao_0.13.3=%s' \
  'rtgi_composicao_0.13.2=%s' \
  'Historico RTGI 0.13.3 criado' \
  'Falha ao criar historico RTGI 0.13.3' \
  'Historico RTGI 0.13.3 descartado'; do
  if ! grep -Fq "${rtgi_message}" "${dxgi_strings}"; then
    echo "Modulo RTGI ausente no nucleo DXGI: ${rtgi_message}" >&2
    exit 1
  fi
done

# Os hashes de depth-preview, ssao e temporal mudaram na 0.12.0: os tres
# perderam suas copias de linearize_reversed_depth/reconstruct_view_* para o
# header compartilhado depth_view_space.hlsli. A igualdade foi provada em
# bytecode com tools/shader_check.sh: ssao e temporal ficaram byte-identicos,
# e depth-preview mudou apenas por +1 max (guarda do rsqrt) e +2 lt/+2 movc
# (validade da normal, que antes nao existia).
#
# O header entra no pino porque agora e a unica fonte da matematica usada
# pelos tres shaders aprovados: alterar so ele mudaria os tres em silencio.
depth_view_space_header="${project_dir}/shaders/depth_view_space.hlsli"
expected_depth_view_space_sha256="e0e6f4ce484186b80a5e9ed676cbe22272f5ea188c1afba9f1a8fc93a36e1192"
actual_depth_view_space_sha256="$(sha256sum "${depth_view_space_header}" | awk '{print $1}')"
if [[ "${actual_depth_view_space_sha256}" != "${expected_depth_view_space_sha256}" ]]; then
  echo "Header depth/view-space aprovado foi alterado: ${actual_depth_view_space_sha256}" >&2
  exit 1
fi

visual_shader="${project_dir}/shaders/photorealism.hlsl"
expected_visual_shader_sha256="d748d8b92645f847ce2a8431f97187b089e5aad34e349f31d6a4c87bfc2c5c95"
actual_visual_shader_sha256="$(sha256sum "${visual_shader}" | awk '{print $1}')"
if [[ "${actual_visual_shader_sha256}" != "${expected_visual_shader_sha256}" ]]; then
  echo "Shader visual aprovado foi alterado: ${actual_visual_shader_sha256}" >&2
  exit 1
fi

depth_preview_shader="${project_dir}/shaders/depth-preview.hlsl"
expected_depth_preview_shader_sha256="e12de14a45ce2781507963c8834ca53a1503aa25ed58a5e90b51ffd02c7b0f61"
actual_depth_preview_shader_sha256="$(sha256sum "${depth_preview_shader}" | awk '{print $1}')"
if [[ "${actual_depth_preview_shader_sha256}" != "${expected_depth_preview_shader_sha256}" ]]; then
  echo "Shader depth preview aprovado foi alterado: ${actual_depth_preview_shader_sha256}" >&2
  exit 1
fi

ssao_shader="${project_dir}/shaders/ssao.hlsl"
expected_ssao_shader_sha256="97e0434b739789210eb4e77896b21d55301f331309f3c89779a6bf74fe050314"
actual_ssao_shader_sha256="$(sha256sum "${ssao_shader}" | awk '{print $1}')"
if [[ "${actual_ssao_shader_sha256}" != "${expected_ssao_shader_sha256}" ]]; then
  echo "Shader SSAO aprovado foi alterado: ${actual_ssao_shader_sha256}" >&2
  exit 1
fi

temporal_shader="${project_dir}/shaders/temporal.hlsl"
expected_temporal_shader_sha256="999b9766bd3f391a121e70421c204ba4a3a5a8dea14f4792f81d9d71d81b7181"
actual_temporal_shader_sha256="$(sha256sum "${temporal_shader}" | awk '{print $1}')"
if [[ "${actual_temporal_shader_sha256}" != "${expected_temporal_shader_sha256}" ]]; then
  echo "Shader temporal aprovado foi alterado: ${actual_temporal_shader_sha256}" >&2
  exit 1
fi

declare -A fsr_official_hashes=(
  ["third_party/fidelityfx-fsr/ffx_a.h"]="f60e2722fcd13989523b9164d776ab382b3692791767f3bf8bb19967f763f3fb"
  ["third_party/fidelityfx-fsr/ffx_fsr1.h"]="93c3922362ea7fc99cbcc698ca30c98de4f8c246d1fbb0b09e015ddef38ce3a5"
  ["third_party/fidelityfx-fsr/LICENSE.txt"]="db089274ce766da70f5b7d791029c3486f9f9e27c8c79c652689603d3192e802"
)
for relative in "${!fsr_official_hashes[@]}"; do
  actual="$(sha256sum "${project_dir}/${relative}" | awk '{print $1}')"
  if [[ "${actual}" != "${fsr_official_hashes[${relative}]}" ]]; then
    echo "Fonte oficial FidelityFX-FSR alterada: ${relative} ${actual}" >&2
    exit 1
  fi
done

# A elegibilidade do depth de camera: forma e veto, tamanho nativo e
# suficiente, e o log diz por que cada candidato caiu.
for depth_eligibility_marker in \
  'kMinimumSceneAreaPercent' \
  'is_plausible_scene_shape' \
  'depth_candidate_rejection' \
  'forma-incompativel'; do
  if ! grep -Fq "${depth_eligibility_marker}" \
      "${project_dir}/src/depth_scoring.hpp"; then
    echo "Elegibilidade de depth incompleta: ${depth_eligibility_marker}" >&2
    exit 1
  fi
done
if ! grep -Fq 'elegibilidade=%s' \
    "${project_dir}/src/resource_observer.cpp"; then
  echo "Motivo de rejeicao ausente no log de recursos depth." >&2
  exit 1
fi

depth_scoring_test="/tmp/photorealism-plugin-depth-scoring-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/depth_scoring_test.cpp" \
  -o "${depth_scoring_test}"
"${depth_scoring_test}"

fsr_color_scoring_test="/tmp/photorealism-fsr-color-scoring-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/fsr_color_scoring_test.cpp" \
  -o "${fsr_color_scoring_test}"
"${fsr_color_scoring_test}"

native_aa_config_test="/tmp/photorealism-native-aa-config-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/native_aa_config_test.cpp" \
  -o "${native_aa_config_test}"
"${native_aa_config_test}"

fsr_draw_shape_test="/tmp/photorealism-fsr-draw-shape-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/fsr_draw_shape_test.cpp" \
  -o "${fsr_draw_shape_test}"
"${fsr_draw_shape_test}"

fsr_rejected_draw_identity_test="/tmp/photorealism-fsr-rejected-draw-identity-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/fsr_rejected_draw_identity_test.cpp" \
  -o "${fsr_rejected_draw_identity_test}"
"${fsr_rejected_draw_identity_test}"

rtgi_config_test="/tmp/photorealism-rtgi-config-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/rtgi_config_test.cpp" \
  -o "${rtgi_config_test}"
"${rtgi_config_test}"

screenshot_request_gate_test="/tmp/photorealism-screenshot-request-gate-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/screenshot_request_gate_test.cpp" \
  -o "${screenshot_request_gate_test}"
"${screenshot_request_gate_test}"

effective_profile="$(awk -F= '
  /^\[/ { section=$0; next }
  /^[[:space:]]*(#|;|$)/ { next }
  {
    key=$1
    value=$2
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", key)
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", value)
    if (key == "enabled") next
    if (section == "[base.0.1.2]") {
      total[key]=value + 0
    } else if (section ~ /^\[module\./) {
      sub(/_delta$/, "", key)
      total[key]+=value + 0
    }
  }
  END {
    printf "%.1f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f", \
      total["temperature"], total["exposure"], total["contrast"], \
      total["saturation"], total["vibrance"], total["shadows"], \
      total["highlights"], total["blacks"], total["whites"], \
      total["local_contrast"], total["sharpness"], total["vignette"]
  }
' "${cfg}")"

# O hash fecha o cfg depois das guardas por chave, e nao antes.
#
# Ate a 0.13.3 ele vinha primeiro, e por isso nenhuma das guardas nomeadas
# acima chegava a falar: qualquer edicao do arquivo batia no hash e saia com
# "Configuracao consolidada foi alterada", que nao diz o que quebrou nem por
# que importa. Uma guarda que explica uma regressao sutil so serve se for ela
# a falar. Nesta ordem o hash continua pegando tudo que as guardas nao
# cobrem, e so isso.
expected_cfg_sha256="3d98bdd4d3e24d67ff2e615250001d1801bd726dabca5c9f165c970e97c04ba0"
actual_cfg_sha256="$(sha256sum "${cfg}" | awk '{print $1}')"
if [[ "${actual_cfg_sha256}" != "${expected_cfg_sha256}" ]]; then
  echo "Configuracao consolidada foi alterada: ${actual_cfg_sha256}" >&2
  exit 1
fi
expected_profile="6400.0 -0.030 1.070 0.970 0.050 0.100 -0.180 -0.060 0.080 0.240 0.200 0.030"
if [[ "${effective_profile}" != "${expected_profile}" ]]; then
  echo "Perfil cumulativo divergiu da 0.3.0 aprovada: ${effective_profile}" >&2
  exit 1
fi

if command -v glslangValidator >/dev/null 2>&1; then
  glslangValidator -D -S vert -e VSMain -V \
    "${project_dir}/shaders/photorealism.hlsl" \
    -o /tmp/photorealism-plugin-vs.spv >/dev/null
  glslangValidator -D -S frag -e PSMain -V \
    "${project_dir}/shaders/photorealism.hlsl" \
    -o /tmp/photorealism-plugin-ps.spv >/dev/null
  glslangValidator -D -S frag -e PSDepthPreview -V \
    "${project_dir}/shaders/depth-preview.hlsl" \
    -o /tmp/photorealism-plugin-depth-preview.spv >/dev/null
  glslangValidator -D -S frag -e PSSSAO -V \
    "${project_dir}/shaders/ssao.hlsl" \
    -o /tmp/photorealism-plugin-ssao.spv >/dev/null
  glslangValidator -D -S frag -e PSTemporal -V \
    "${project_dir}/shaders/temporal.hlsl" \
    -o /tmp/photorealism-plugin-temporal.spv >/dev/null
  glslangValidator -D -S comp -e CSEasu -V \
    "${project_dir}/shaders/fsr1.hlsl" \
    -o /tmp/photorealism-fsr-easu.spv >/dev/null
  glslangValidator -D -S comp -e CSTemporalAa -V \
    "${project_dir}/shaders/fsr1.hlsl" \
    -o /tmp/photorealism-aa-temporal.spv >/dev/null
  glslangValidator -D -S comp -e CSRcas -V \
    "${project_dir}/shaders/fsr1.hlsl" \
    -o /tmp/photorealism-fsr-rcas.spv >/dev/null
fi

native_aa_source="${project_dir}/src/native_aa_config.cpp"
for native_aa_marker in \
  'eurotrucks2.exe' \
  'amtrucks.exe' \
  'config.photorealism-native-aa.backup.cfg' \
  'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH' \
  'kNativeAaSection = "native_aa.0.12.2"' \
  'read_native_aa_policy' \
  'policy.manage' \
  'policy.aa.c_str()' \
  'policy.taa_sharpen.c_str()' \
  'plugin_config_value' \
  'nenhuma alteracao no ' \
  'politica=photorealism-plugin.cfg'; do
  if ! grep -Fq "${native_aa_marker}" "${native_aa_source}"; then
    echo "Gestao automatica AA nativo incompleta: ${native_aa_marker}" >&2
    exit 1
  fi
done

# O numero de versao e carimbo de chegada, nao de agenda. Duas vezes o ROADMAP
# prometeu uma entrega num numero que um pacote ja tinha consumido -- a fase de
# composicao do RTGI presa na 0.12.2, que virou a politica de AA nativo, e a
# recalibracao do SSAO presa na 0.13.1, que virou o conserto do Page Down.
# Promessa apontando para numero ja entregue e documentacao que mente.
changelog="${project_dir}/CHANGELOG.md"
roadmap="${project_dir}/ROADMAP.md"
shipped_versions="$(grep -E '^## ' "${changelog}" \
  | grep -oE '^## (Pacote |Core )?[0-9]+\.[0-9]+\.[0-9]+' \
  | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | sort -Vu)"
promised_versions="$(grep -E '^- \*\*[0-9]+\.[0-9]+\.[0-9]+' "${roadmap}" \
  | grep -v '(entregue)' \
  | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | sort -Vu)"
version_collisions="$(printf '%s\n' "${promised_versions}" \
  | grep -Fxf <(printf '%s\n' "${shipped_versions}") || true)"
if [[ -n "${version_collisions}" ]]; then
  echo "ROADMAP promete versao ja entregue no CHANGELOG: \
${version_collisions//$'\n'/ }" >&2
  echo "Renumere as fases nao entregues do ROADMAP." >&2
  exit 1
fi
if ! grep -Fq 'carimbo de' "${roadmap}"; then
  echo "ROADMAP sem a regra de renumeracao das fases nao entregues." >&2
  exit 1
fi

# A 0.13.2 e o que faz o RTGI alterar a imagem. Tres coisas nao podem sumir sem
# a versao deixar de entregar o que promete: a marcha geometrica (sem ela o
# ponto cego de 0,5 a 1,71 m volta e a cabine fica fora do alcance), a
# cobertura do interior como teste, e o passe de composicao.
for rtgi_marker in \
  'rtgi_step_ratio' \
  'rtgi_sample_distance' \
  'rtgi_samples_within'; do
  if ! grep -Fq "${rtgi_marker}" "${project_dir}/src/rtgi_config.hpp"; then
    echo "Marcha geometrica RTGI 0.13.2 incompleta: ${rtgi_marker}" >&2
    exit 1
  fi
done
if grep -Fq 'rtgi_step_size' "${project_dir}/src/rtgi_config.hpp"; then
  echo "rtgi_step_size sobreviveu a 0.13.2; o passo fixo foi substituido." >&2
  exit 1
fi
grep -Fq 'PSRtgiCompose' "${project_dir}/shaders/rtgi.hlsl"

# A 0.13.2.1 e o que faz o RTGI alcancar o interior. march_ray tem quatro
# desfechos e so um deles e acerto real; os outros tres -- fora da tela, ceu, e
# escape (passos esgotados ou plano proximo cruzado) -- precisam devolver o
# MESMO termo de ambiente. Ate a 0.13.2 dois devolviam SkyAmbient e um devolvia
# preto, e com sky_ambient=0.0 os tres devolviam preto.
if ! grep -Fq 'float3 ambient_escape(float3 direction)' \
  "${project_dir}/shaders/rtgi.hlsl"; then
  echo "ambient_escape sumiu de rtgi.hlsl: os desfechos de nao-acerto perderam \
o termo de ambiente comum." >&2
  exit 1
fi
ambient_escape_sites="$(grep -c 'result.indirect = ambient_escape(direction);' \
  "${project_dir}/shaders/rtgi.hlsl")"
if [[ "${ambient_escape_sites}" != "3" ]]; then
  echo "Escapes de march_ray com ambiente: ${ambient_escape_sites}, \
esperado 3. Algum desfecho voltou a devolver preto." >&2
  exit 1
fi
grep -Fq 'sky_ambient > 0.0f' "${project_dir}/tests/rtgi_config_test.cpp"
grep -Fq 'PSRtgiCompose' "${project_dir}/src/postprocess.cpp"
grep -Fq 'render_rtgi_compose_pass' "${project_dir}/src/postprocess.cpp"
grep -Fq 'rtgi_samples_within' "${project_dir}/tests/rtgi_config_test.cpp"

# A 0.13.3 e o que torna quatro raios por pixel um orcamento viavel: sem
# acumular frames, o sorteio de direcao de cada raio vai inteiro para a tela
# como cintilacao.
if ! grep -Fq 'float4 PSRtgiTemporal(VertexOutput input) : SV_Target' \
  "${project_dir}/shaders/rtgi.hlsl"; then
  echo "PSRtgiTemporal sumiu de rtgi.hlsl: o GI volta a ser composto raio a \
raio, sem acumulacao." >&2
  exit 1
fi
# O hash por raio precisa ser inteiro. O sin() que estava aqui ate a 0.13.2.1
# perde os bits baixos conforme o argumento cresce com o frame -- degrada
# justamente ao longo dos minutos em que a acumulacao deveria estar somando.
if ! grep -Fq 'uint pcg_hash(uint value)' "${project_dir}/shaders/rtgi.hlsl"
then
  echo "pcg_hash sumiu de rtgi.hlsl: o hash por raio volta a degradar com o \
frame, e a acumulacao soma amostras cada vez piores." >&2
  exit 1
fi
if ! grep -Fq 'float2 ray_random(uint2 pixel, uint frame, uint ray_index)' \
  "${project_dir}/shaders/rtgi.hlsl"; then
  echo "ray_random deixou de receber pixel, frame e raio como inteiros; e por \
ai que o hash de ponto flutuante volta." >&2
  exit 1
fi
# A constante existir nao basta: o que importa e o azimute ser girado por ela a
# cada frame. Por isso a guarda e sobre o uso, e nao sobre o nome.
if ! grep -Fq 'random.y = frac(random.y + frame_rotation);' \
  "${project_dir}/shaders/rtgi.hlsl"; then
  echo "A rotacao por frame sumiu de rtgi.hlsl: sem ela a acumulacao converge \
para a media de amostras mal distribuidas." >&2
  exit 1
fi
for rtgi_temporal_marker in \
  'rtgi_history_alpha' \
  'ensure_rtgi_temporal_resources' \
  'render_rtgi_temporal_pass' \
  'invalidate_rtgi_history'; do
  if ! grep -Fq "${rtgi_temporal_marker}" \
    "${project_dir}/src/postprocess.cpp" \
    "${project_dir}/src/rtgi_config.hpp"; then
    echo "Acumulacao RTGI 0.13.3 incompleta: ${rtgi_temporal_marker}" >&2
    exit 1
  fi
done
# As duas regressoes da 0.13.3 que so o teste pode provar. Guardas nomeadas, e
# nao greps nus: um grep nu sob `set -e` derruba a validacao sem dizer nada, e
# uma guarda muda nao guarda coisa alguma.
rtgi_test="${project_dir}/tests/rtgi_config_test.cpp"
# A asserta em si, e nao o nome: com o nome bastando, a declaracao `using` no
# topo do teste ja satisfaria a guarda com zero cobertura.
if ! grep -Fq 'rtgi_history_alpha(0.90f, 1.0f, 0.0f, 1.0f) == 0.0f' \
  "${rtgi_test}"; then
  echo "O teste parou de provar que UMA rejeicao zerada descarta a historia \
inteira: e o que separa acumular de borrar." >&2
  exit 1
fi
if ! grep -Fq 'normal_rejection > 0.0f' "${rtgi_test}"; then
  echo "O teste parou de exigir normal_rejection maior que zero: com ele em \
zero a historia e aceita de qualquer normal, que e ghosting na quina do \
painel contra o para-brisa." >&2
  exit 1
fi

echo "Proxies, core Photorealism, captura Steam, draw proof FSR 0.7.1, depth, SSAO, telemetria, perfil, shaders e numeracao de versao validados."
